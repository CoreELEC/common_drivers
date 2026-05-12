// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define INFO_PREFIX "video_rdma"
#define pr_fmt(fmt) "rdma: " fmt
#define DEBUG

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/dma-mapping.h>

#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/ctype.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/clk.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include "rdma.h"
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/registers/register_map.h>
#include <linux/amlogic/media/rdma/rdma_mgr.h>
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#include <linux/amlogic/media/video_sink/video.h>
#endif
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/utils/am_com.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>

#define Wr(adr, val) WRITE_VCBUS_REG(adr, val)
#define Rd(adr)     READ_VCBUS_REG(adr)
#define Wr_reg_bits(adr, val, start, len) \
			WRITE_VCBUS_REG_BITS(adr, val, start, len)

#define RDMA_NUM  7
#define VIU_OSD4_MISC   0x1a18 /* used as rdma_done_detect register */

static int second_rdma_feature;
int vsync_rdma_handle[RDMA_NUM];
static int irq_count[RDMA_NUM];
static int enable[RDMA_NUM];
static int cur_enable[RDMA_NUM];
static int pre_enable_[RDMA_NUM];
static int debug_flag[RDMA_NUM];
static int vsync_cfg_count[RDMA_NUM];
static u32 force_rdma_config[RDMA_NUM];
static bool first_config[RDMA_NUM];
static bool rdma_done[RDMA_NUM];
static u32 cur_vsync_handle_id;
static int ex_vsync_rdma_enable;
ulong rdma_done_us[RDMA_NUM];
ulong rdma_vsync_us[RDMA_NUM];
unsigned int rdma_done_detect_reg;
unsigned int rdma_done_detect_cnt;
unsigned int use_rdma_done_detect;
unsigned int no_rdma_done_max_cnt = 5;
struct lowlatency_reg_s lowlatency_reg;

static DEFINE_SPINLOCK(lock);
static void vsync_rdma_irq(void *arg);
static void vsync_rdma_vpp1_irq(void *arg);
static void vsync_rdma_vpp2_irq(void *arg);
static void pre_vsync_rdma_irq(void *arg);
static void line_n_int_rdma_irq(void *arg);
static void vsync_rdma_read_irq(void *arg);
static void ex_vsync_rdma_irq(void *arg);

struct rdma_op_s vsync_rdma_op = {
	vsync_rdma_irq,
	NULL
};

struct rdma_op_s vsync_rdma_vpp1_op = {
	vsync_rdma_vpp1_irq,
	NULL
};

struct rdma_op_s vsync_rdma_vpp2_op = {
	vsync_rdma_vpp2_irq,
	NULL
};

struct rdma_op_s pre_vsync_rdma_op = {
	pre_vsync_rdma_irq,
	NULL
};

struct rdma_op_s line_n_int_rdma_op = {
	line_n_int_rdma_irq,
	NULL
};

struct rdma_op_s vsync_rdma_read_op = {
	vsync_rdma_read_irq,
	NULL
};

struct rdma_op_s ex_vsync_rdma_op = {
	ex_vsync_rdma_irq,
	NULL
};

inline int VSYNC_WR_TABLE_REG_SIMPLE(int tbl_idx, u32 adr, u32 val)
{
	int enable_ = cur_enable[cur_vsync_handle_id] & 0xf;

	if (enable_ != 0 && vsync_rdma_handle[cur_vsync_handle_id] > 0) {
		if (cur_vsync_handle_id == VSYNC_RDMA) {
			if (get_part_flag_status(RDMA_VPP0, tbl_idx))
				rdma_part_write_reg_simple(tbl_idx,
						vsync_rdma_handle[cur_vsync_handle_id], adr, val);
		} else if (cur_vsync_handle_id == EX_VSYNC_RDMA) {
			if (get_part_flag_status(RDMA_LOWLATENCY, tbl_idx))
				rdma_part_write_reg_simple(tbl_idx,
						vsync_rdma_handle[cur_vsync_handle_id], adr, val);
		} else {
			rdma_write_reg(vsync_rdma_handle[cur_vsync_handle_id], adr, val);
		}
	} else {
		Wr(adr, val);
		if (debug_flag[cur_vsync_handle_id] & 1)
			pr_info("VSYNC_WR(%x)=%x\n", adr, val);
	}
	return 0;
}
EXPORT_SYMBOL(VSYNC_WR_TABLE_REG_SIMPLE);

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
static void mark_lowlatency_channel(int handle)
{
	lowlatency_reg.rdma_handle = handle;
	if (debug_flag[handle] & 8)
		pr_info("%s handle=%d\n", __func__, handle);
}
#endif

#ifndef CONFIG_AMLOGIC_MEDIA_VIDEO
u32 get_enc_num_start(u8 index)
{
	return 0;
}

u32 get_cur_enc_num(void)
{
	return 0;
}

ulong get_enc_time_start(u8 index)
{
	return 0;
}

u32 get_cur_enc_line(void)
{
	return 0;
}
#endif

int get_ex_vsync_rdma_enable(void)
{
	return ex_vsync_rdma_enable;
}

void set_ex_vsync_rdma_enable(int enable)
{
	ex_vsync_rdma_enable = enable;
}

static void set_rdma_trigger_line(void)
{
	int trigger_line;

	switch (aml_read_vcbus(VPU_VIU_VENC_MUX_CTRL) & 0x3) {
	case 0:
		trigger_line = aml_read_vcbus(ENCL_VIDEO_VAVON_ELINE)
			- aml_read_vcbus(ENCL_VIDEO_VSO_BLINE);
		break;
	case 1:
		if ((aml_read_vcbus(ENCI_VIDEO_MODE) & 1) == 0)
			trigger_line = 260; /* 480i */
		else
			trigger_line = 310; /* 576i */
		break;
	case 2:
		if (aml_read_vcbus(ENCP_VIDEO_MODE) & (1 << 12))
			trigger_line = aml_read_vcbus(ENCP_DE_V_END_EVEN);
		else
			trigger_line = aml_read_vcbus(ENCP_VIDEO_VAVON_ELINE)
				- aml_read_vcbus(ENCP_VIDEO_VSO_BLINE);
		break;
	case 3:
		trigger_line = aml_read_vcbus(ENCT_VIDEO_VAVON_ELINE)
			- aml_read_vcbus(ENCT_VIDEO_VSO_BLINE);
		break;
	}
	aml_write_vcbus(VPP_INT_LINE_NUM, trigger_line);
}

static inline int is_rdma_done(int rdma_type)
{
	unsigned int hw_done_bit = rdma_hw_done_bit();
	int ret = 1;

	if (!rdma_done[rdma_type] &&
	    !(hw_done_bit & (1 << vsync_rdma_handle[rdma_type])))
		ret = 0;

	if (debug_flag[rdma_type] & 2)
		pr_devel("%s, rdma_type:%d rdma_done:%d hw_done_bit:0x%x\n",
			 __func__, rdma_type, rdma_done[rdma_type],
			 hw_done_bit);

	return ret;
}

/*
 * Description: Determine whether to execute RDMA configuration.
 * Return:
 *    0: Do not execute RDMA configuration.
 *    1: Execute RDMA configuration.
 */
int need_to_rdma_config(int rdma_type)
{
	int run_config = 1;

	if (rdma_type != PRE_VSYNC_RDMA) {
		pr_info("%s, %d not supported\n", __func__, rdma_type);
		return 1;
	}

	if (!use_rdma_done_detect)
		return 1;

	if (rdma_configured[rdma_type]) {
		struct vinfo_s *vinfo = NULL;
		u32 done_detect_cnt = 0, run_case = 0;
		u32 enc_num_start = 0, enc_num_cur = 0, enc_num_cur_tmp = 0;
		ulong t = 0, enc_time_start = 0, enc_time_cur = 0;
		ulong sync_interval = 16666, interval = 0, enc_num_cycle_time;
		static int no_rdma_done;
		struct timeval cur;

		#ifdef CONFIG_AMLOGIC_VOUT_SERVE
		vinfo = get_current_vinfo();
		#endif
		if (vinfo)
			sync_interval = vinfo->sync_duration_den * 1000000 /
						vinfo->sync_duration_num;

		enc_num_start = get_enc_num_start(1);
		enc_num_cur = get_cur_enc_num();

		/* cycling from 0 to 7 */
		if (enc_num_cur < enc_num_start)
			enc_num_cur_tmp = enc_num_cur + 8;
		else
			enc_num_cur_tmp = enc_num_cur;
		enc_num_cycle_time = sync_interval * 7;

		do_gettimeofday(&cur);
		enc_time_start = get_enc_time_start(1);
		enc_time_cur = cur.tv_sec * 1000000 + cur.tv_usec;
		interval = enc_time_cur - enc_time_start;
		t = abs(enc_time_start - rdma_config_us[rdma_type]);

		/* check rdma_done
		 * make sure the last configuration is completed
		 */
		if (!is_rdma_done(rdma_type)) {
			run_config = 0;
			no_rdma_done++;
			run_case = 1;
			goto check_count;
		}
		/* check enc num
		 * avoid multiple configurations in one vsync
		 */
		if (enc_num_configed[rdma_type] == enc_num_start &&
		    t < enc_num_cycle_time) {
			run_config = 0;
			no_rdma_done++;
			run_case = 2;
			goto check_count;
		}
		/* check done_detect_cnt
		 * make sure all registers in the table are completed
		 */
		done_detect_cnt = READ_VCBUS_REG(rdma_done_detect_reg);
		if (done_detect_cnt != rdma_done_detect_cnt) {
			run_config = 0;
			no_rdma_done++;
			run_case = 3;
			goto check_count;
		}

check_count:
		/* check no_rdma_count */
		if (no_rdma_done >= no_rdma_done_max_cnt) {
			run_config = 1;
			no_rdma_done = 0;
			run_case = 4;
		}

		/* do rdma_stop for the next configuration */
		if (run_config) {
			no_rdma_done = 0;
			rdma_stop(vsync_rdma_handle[PRE_VSYNC_RDMA]);
		}
		if (debug_flag[rdma_type] & 2) {
			pr_devel("%s enc_num(configed:%d start:%d cur:%d(%d)) interval:%lu sync_interval:%lu\n",
				 __func__, enc_num_configed[rdma_type],
				 enc_num_start, enc_num_cur, enc_num_cur_tmp,
				 interval, sync_interval);
			pr_devel("no_rdma_done:%d case:%d configed:%d run_config:%d\n",
				 no_rdma_done, run_case,
				 rdma_configured[rdma_type], run_config);
		}
	}

	return run_config;
}

int _vsync_rdma_config(int rdma_type)
{
	int iret = 0;
	int enable_ = cur_enable[rdma_type] & 0xf;
	unsigned long flags;
	struct timeval t;
	int config = 1, handle;

	if (vsync_rdma_handle[rdma_type] <= 0)
		return -1;

	do_gettimeofday(&t);
	rdma_vsync_us[rdma_type] = t.tv_sec * 1000000 + t.tv_usec;

	/* first frame not use rdma */
	if (!first_config[rdma_type]) {
		cur_enable[rdma_type] = enable[rdma_type];
		pre_enable_[rdma_type] = cur_enable[rdma_type];
		first_config[rdma_type] = true;
		rdma_done[rdma_type] = false;
		rdma_clear(vsync_rdma_handle[rdma_type]);
		force_rdma_config[rdma_type] = 1;
		return 0;
	}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (rdma_type == EX_VSYNC_RDMA &&
	    is_video_process_in_thread() && vpp_low_latency_check())
		return -2;
#endif
	if (rdma_type == EX_VSYNC_RDMA) {
		spin_lock_irqsave(&lock, flags);
		force_rdma_config[rdma_type] = 1;
	}
	if (use_rdma_done_detect && rdma_type == PRE_VSYNC_RDMA) {
		config = need_to_rdma_config(rdma_type);
		force_rdma_config[rdma_type] = 1;
	}

	/* if rdma mode changed, reset rdma */
	if (pre_enable_[rdma_type] != enable_) {
		rdma_clear(vsync_rdma_handle[rdma_type]);
		force_rdma_config[rdma_type] = 1;
	}

	if (force_rdma_config[rdma_type])
		rdma_done[rdma_type] = true;

	handle = vsync_rdma_handle[rdma_type];
	if (enable_ == 1) {
		if (rdma_done[rdma_type])
			iret = rdma_watchdog_setting(0, handle);
		else
			iret = rdma_watchdog_setting(1, handle);
	} else {
		/* not vsync mode */
		iret = rdma_watchdog_setting(0, handle);
		force_rdma_config[rdma_type] = 1;
	}
	rdma_done[rdma_type] = false;
	if (iret)
		force_rdma_config[rdma_type] = 1;

	iret = 0;
	if (force_rdma_config[rdma_type] ||
	    rdma_reset_trigger_flag[handle]) {
		if (enable_ == 1) {
			if (has_multi_vpp) {
				if (rdma_type == VSYNC_RDMA) {
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
					if (get_lowlatency_mode() &&
						get_rdma_item_count(vsync_rdma_handle[rdma_type]))
						mark_lowlatency_channel
							(vsync_rdma_handle[rdma_type]);
#endif
					iret = rdma_config(vsync_rdma_handle[rdma_type],
							   RDMA_TRIGGER_VSYNC_INPUT);
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
					if (iret)
						update_over_field_states(OVER_FIELD_RDMA_READY,
							false);
#endif
				} else if (rdma_type == VSYNC_RDMA_VPP1) {
					iret = rdma_config(vsync_rdma_handle[rdma_type],
							   RDMA_TRIGGER_VPP1_VSYNC_INPUT);
				} else if (rdma_type == VSYNC_RDMA_VPP2) {
					iret = rdma_config(vsync_rdma_handle[rdma_type],
							  RDMA_TRIGGER_VPP2_VSYNC_INPUT);
				} else if (rdma_type == PRE_VSYNC_RDMA &&
					   config) {
					if (is_meson_t3x_cpu())
						iret = rdma_config(vsync_rdma_handle[rdma_type],
							RDMA_TRIGGER_PRE_VSYNC_INPUT_T3X);
					else
						iret = rdma_config(vsync_rdma_handle[rdma_type],
							RDMA_TRIGGER_PRE_VSYNC_INPUT);
				} else if (rdma_type == EX_VSYNC_RDMA) {
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
					if (get_lowlatency_mode() &&
						get_rdma_item_count(vsync_rdma_handle[rdma_type]))
						mark_lowlatency_channel
							(vsync_rdma_handle[rdma_type]);
#endif
					iret = rdma_config(vsync_rdma_handle[rdma_type],
						RDMA_TRIGGER_VSYNC_INPUT |
						RDMA_TRIGGER_OMIT_LOCK);
					if (iret)
						update_over_field_states(OVER_FIELD_RDMA_READY,
							false);
				} else if (rdma_type == LINE_N_INT_RDMA) {
					set_rdma_trigger_line();
					iret = rdma_config(vsync_rdma_handle[rdma_type],
							   RDMA_TRIGGER_LINE_INPUT);
				} else if (read_rdma_en && rdma_type == VSYNC_RDMA_READ)
					iret = rdma_config(vsync_rdma_handle[rdma_type],
							   read_rdma_trigger |
							   RDMA_READ_MASK);
			} else {
				if (rdma_type == VSYNC_RDMA) {
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
					if (get_lowlatency_mode() &&
						get_rdma_item_count(vsync_rdma_handle[rdma_type]))
						mark_lowlatency_channel
							(vsync_rdma_handle[rdma_type]);
#endif
					iret = rdma_config(vsync_rdma_handle[rdma_type],
							   RDMA_TRIGGER_VSYNC_INPUT);
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
					if (iret)
						update_over_field_states(OVER_FIELD_RDMA_READY,
							false);
#endif
				} else if (rdma_type == LINE_N_INT_RDMA) {
					set_rdma_trigger_line();
					iret = rdma_config(vsync_rdma_handle[rdma_type],
							   RDMA_TRIGGER_LINE_INPUT);
				} else if (rdma_type == VSYNC_RDMA_READ) {
					iret = rdma_config(vsync_rdma_handle[rdma_type],
							   read_rdma_trigger |
							   RDMA_READ_MASK);
				} else if (rdma_type == EX_VSYNC_RDMA) {
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
					if (get_lowlatency_mode() &&
						get_rdma_item_count(vsync_rdma_handle[rdma_type]))
						mark_lowlatency_channel
							(vsync_rdma_handle[rdma_type]);
#endif
					iret = rdma_config(vsync_rdma_handle[rdma_type],
						RDMA_TRIGGER_VSYNC_INPUT |
						RDMA_TRIGGER_OMIT_LOCK);
					if (iret)
						update_over_field_states(OVER_FIELD_RDMA_READY,
							false);
				}
			}
			if (iret)
				vsync_cfg_count[rdma_type]++;
		} else if (enable_ == 2) {
			/*manually in cur vsync*/
			rdma_config(vsync_rdma_handle[rdma_type],
				    RDMA_TRIGGER_MANUAL);
		} else if (enable_ == 3) {
			;
		} else if (enable_ == 4) {
			rdma_config(vsync_rdma_handle[rdma_type],
				    RDMA_TRIGGER_DEBUG1); /*for debug*/
		} else if (enable_ == 5) {
			rdma_config(vsync_rdma_handle[rdma_type],
				    RDMA_TRIGGER_DEBUG2); /*for debug*/
		} else if (enable_ == 6) {
			;
		}
		if (!iret) {
			force_rdma_config[rdma_type] = 1;
			iret = 0;
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
			update_over_field_states(OVER_FIELD_RDMA_READY, false);
#endif
		} else {
			force_rdma_config[rdma_type] = 0;
			iret = 0;
			if (rdma_reset_trigger_flag[handle]) {
				rdma_reset_trigger_flag[handle] = 0;
				iret = 1;
			}
		}
	}
	pre_enable_[rdma_type] = enable_;
	cur_enable[rdma_type] = enable[rdma_type];
	if (rdma_type == EX_VSYNC_RDMA)
		spin_unlock_irqrestore(&lock, flags);
	return iret;
}

int vsync_rdma_config(void)
{
	int ret;

	ret = _vsync_rdma_config(cur_vsync_handle_id);
	if (cur_vsync_handle_id == EX_VSYNC_RDMA &&
		 vsync_rdma_handle[cur_vsync_handle_id] > 0)
		rdma_buffer_unlock(vsync_rdma_handle[cur_vsync_handle_id]);

	if (!has_multi_vpp ||
		(read_rdma_en && read_rdma_trigger == 1)) {
		ret = _vsync_rdma_config(VSYNC_RDMA_READ);
		if (second_rdma_feature &&
		    is_meson_g12b_revb())
			ret = _vsync_rdma_config(LINE_N_INT_RDMA);
	} else {
		ret = _vsync_rdma_config(LINE_N_INT_RDMA);
	}
	return ret;
}
EXPORT_SYMBOL(vsync_rdma_config);

int vsync_rdma_vpp1_config(void)
{
	return _vsync_rdma_config(VSYNC_RDMA_VPP1);
}
EXPORT_SYMBOL(vsync_rdma_vpp1_config);

int vsync_rdma_vpp2_config(void)
{
	return _vsync_rdma_config(VSYNC_RDMA_VPP2);
}
EXPORT_SYMBOL(vsync_rdma_vpp2_config);

int pre_vsync_rdma_config(void)
{
	return _vsync_rdma_config(PRE_VSYNC_RDMA);
}
EXPORT_SYMBOL(pre_vsync_rdma_config);
void _vsync_rdma_config_pre(int rdma_type)
{
	int enable_ = cur_enable[rdma_type] & 0xf;

	if (vsync_rdma_handle[rdma_type] <= 0)
		return;
	if (enable_ == 3)/*manually in next vsync*/
		rdma_config(vsync_rdma_handle[rdma_type], 0);
	else if (enable_ == 6)
		rdma_config(vsync_rdma_handle[rdma_type], RDMA_TRIGGER_DEBUG1); /*for debug*/
}

void vsync_rdma_config_pre(void)
{
	if (cur_vsync_handle_id == EX_VSYNC_RDMA &&
		 vsync_rdma_handle[cur_vsync_handle_id] > 0)
		rdma_buffer_lock(vsync_rdma_handle[cur_vsync_handle_id]);
	_vsync_rdma_config_pre(cur_vsync_handle_id);
	if (!has_multi_vpp || read_rdma_en) {
		_vsync_rdma_config_pre(VSYNC_RDMA_READ);
		if (second_rdma_feature &&
		    is_meson_g12b_revb())
			_vsync_rdma_config_pre(LINE_N_INT_RDMA);
	}
}
EXPORT_SYMBOL(vsync_rdma_config_pre);

void vsync_rdma_vpp1_config_pre(void)
{
	_vsync_rdma_config_pre(VSYNC_RDMA_VPP1);
}
EXPORT_SYMBOL(vsync_rdma_vpp1_config_pre);

void vsync_rdma_vpp2_config_pre(void)
{
	_vsync_rdma_config_pre(VSYNC_RDMA_VPP2);
}
EXPORT_SYMBOL(vsync_rdma_vpp2_config_pre);

void pre_vsync_rdma_config_pre(void)
{
	_vsync_rdma_config_pre(PRE_VSYNC_RDMA);
}
EXPORT_SYMBOL(pre_vsync_rdma_config_pre);
static void vsync_rdma_irq(void *arg)
{
	int iret;
	int enable_ = cur_enable[VSYNC_RDMA] & 0xf;
	struct timeval t;

	do_gettimeofday(&t);
	rdma_done_us[VSYNC_RDMA] = t.tv_sec * 1000000 + t.tv_usec;

	if (enable_ == 1) {
		/*triggered by next vsync*/
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
		if (get_lowlatency_mode()) {
			if (get_rdma_item_count(vsync_rdma_handle[VSYNC_RDMA]))
				mark_lowlatency_channel(vsync_rdma_handle[VSYNC_RDMA]);
		}
#endif
		iret = rdma_config(vsync_rdma_handle[VSYNC_RDMA],
				   RDMA_TRIGGER_VSYNC_INPUT);
		if (iret) {
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
			update_over_field_states(OVER_FIELD_RDMA_READY, false);
#endif
			vsync_cfg_count[VSYNC_RDMA]++;
		}
	} else {
		iret = rdma_config(vsync_rdma_handle[VSYNC_RDMA], 0);
	}
	pre_enable_[VSYNC_RDMA] = enable_;

	if (!iret || enable_ != 1)
		force_rdma_config[VSYNC_RDMA] = 1;
	else
		force_rdma_config[VSYNC_RDMA] = 0;

	rdma_done[VSYNC_RDMA] = true;
	irq_count[VSYNC_RDMA]++;
}

static void vsync_rdma_vpp1_irq(void *arg)
{
	int iret;
	int enable_ = cur_enable[VSYNC_RDMA_VPP1] & 0xf;
	struct timeval t;

	do_gettimeofday(&t);
	rdma_done_us[VSYNC_RDMA_VPP1] = t.tv_sec * 1000000 + t.tv_usec;

	if (enable_ == 1) {
		/*triggered by next vsync*/
		iret = rdma_config(vsync_rdma_handle[VSYNC_RDMA_VPP1],
				   RDMA_TRIGGER_VPP1_VSYNC_INPUT);
		if (iret)
			vsync_cfg_count[VSYNC_RDMA_VPP1]++;
	} else {
		iret = rdma_config(vsync_rdma_handle[VSYNC_RDMA_VPP1], 0);
	}
	pre_enable_[VSYNC_RDMA_VPP1] = enable_;

	if (!iret || enable_ != 1)
		force_rdma_config[VSYNC_RDMA_VPP1] = 1;
	else
		force_rdma_config[VSYNC_RDMA_VPP1] = 0;

	rdma_done[VSYNC_RDMA_VPP1] = true;
	irq_count[VSYNC_RDMA_VPP1]++;
}

static void vsync_rdma_vpp2_irq(void *arg)
{
	int iret;
	int enable_ = cur_enable[VSYNC_RDMA_VPP2] & 0xf;
	struct timeval t;

	do_gettimeofday(&t);
	rdma_done_us[VSYNC_RDMA_VPP2] = t.tv_sec * 1000000 + t.tv_usec;

	if (enable_ == 1) {
		/*triggered by next vsync*/
		iret = rdma_config(vsync_rdma_handle[VSYNC_RDMA_VPP2],
				   RDMA_TRIGGER_VPP2_VSYNC_INPUT);
		if (iret)
			vsync_cfg_count[VSYNC_RDMA_VPP2]++;
	} else {
		iret = rdma_config(vsync_rdma_handle[VSYNC_RDMA_VPP2], 0);
	}
	pre_enable_[VSYNC_RDMA_VPP2] = enable_;

	if (!iret || enable_ != 1)
		force_rdma_config[VSYNC_RDMA_VPP2] = 1;
	else
		force_rdma_config[VSYNC_RDMA_VPP2] = 0;

	rdma_done[VSYNC_RDMA_VPP2] = true;
	irq_count[VSYNC_RDMA_VPP2]++;
}

static void pre_vsync_rdma_irq(void *arg)
{
	int iret;
	int enable_ = cur_enable[PRE_VSYNC_RDMA] & 0xf;
	struct timeval t;

	do_gettimeofday(&t);
	rdma_done_us[PRE_VSYNC_RDMA] = t.tv_sec * 1000000 + t.tv_usec;
	if (use_rdma_done_detect)
		goto not_to_config;

	if (enable_ == 1) {
		if (is_meson_t3x_cpu())
			iret = rdma_config(vsync_rdma_handle[PRE_VSYNC_RDMA],
				RDMA_TRIGGER_PRE_VSYNC_INPUT_T3X);
		else
			iret = rdma_config(vsync_rdma_handle[PRE_VSYNC_RDMA],
				RDMA_TRIGGER_PRE_VSYNC_INPUT);
		if (iret)
			vsync_cfg_count[PRE_VSYNC_RDMA]++;
	} else {
		iret = rdma_config(vsync_rdma_handle[PRE_VSYNC_RDMA], 0);
	}
	pre_enable_[PRE_VSYNC_RDMA] = enable_;
	if (!iret || enable_ != 1)
		force_rdma_config[PRE_VSYNC_RDMA] = 1;
	else
		force_rdma_config[PRE_VSYNC_RDMA] = 0;

not_to_config:
	rdma_done[PRE_VSYNC_RDMA] = true;
	irq_count[PRE_VSYNC_RDMA]++;
}

static void line_n_int_rdma_irq(void *arg)
{
	int iret;
	int enable_ = cur_enable[LINE_N_INT_RDMA] & 0xf;

	if (enable_ == 1) {
		/*triggered by next vsync*/
		//set_rdma_trigger_line();
		iret = rdma_config(vsync_rdma_handle[LINE_N_INT_RDMA],
				   RDMA_TRIGGER_LINE_INPUT);
		if (iret)
			vsync_cfg_count[LINE_N_INT_RDMA]++;
	} else {
		iret = rdma_config(vsync_rdma_handle[LINE_N_INT_RDMA], 0);
	}
	pre_enable_[LINE_N_INT_RDMA] = enable_;

	if (!iret || enable_ != 1)
		force_rdma_config[LINE_N_INT_RDMA] = 1;
	else
		force_rdma_config[LINE_N_INT_RDMA] = 0;
	rdma_done[LINE_N_INT_RDMA] = true;
	irq_count[LINE_N_INT_RDMA]++;
}

static void vsync_rdma_read_irq(void *arg)
{
	int iret;
	int enable_ = cur_enable[VSYNC_RDMA_READ] & 0xf;

	if (enable_ == 1) {
		/*triggered by next vsync*/
		iret = rdma_config(vsync_rdma_handle[VSYNC_RDMA_READ],
				   read_rdma_trigger | RDMA_READ_MASK);
		if (iret)
			vsync_cfg_count[VSYNC_RDMA_READ]++;
	} else {
		iret = rdma_config(vsync_rdma_handle[VSYNC_RDMA_READ], 0);
	}
	pre_enable_[VSYNC_RDMA_READ] = enable_;

	if (!iret || enable_ != 1)
		force_rdma_config[VSYNC_RDMA_READ] = 1;
	else
		force_rdma_config[VSYNC_RDMA_READ] = 0;
	rdma_done[VSYNC_RDMA_READ] = true;
	irq_count[VSYNC_RDMA_READ]++;
}

static void ex_vsync_rdma_irq(void *arg)
{
	unsigned long flags;
	int iret;
	int enable_ = cur_enable[EX_VSYNC_RDMA] & 0xf;

	spin_lock_irqsave(&lock, flags);
	if (enable_ == 1) {
		/*triggered by next vsync*/
		iret = rdma_config(vsync_rdma_handle[EX_VSYNC_RDMA],
			RDMA_TRIGGER_VSYNC_INPUT);
		if (iret)
			vsync_cfg_count[EX_VSYNC_RDMA]++;
	} else {
		iret = rdma_config(vsync_rdma_handle[EX_VSYNC_RDMA], 0);
	}
	pre_enable_[EX_VSYNC_RDMA] = enable_;

	if (!iret || enable_ != 1)
		force_rdma_config[EX_VSYNC_RDMA] = 1;
	else
		force_rdma_config[EX_VSYNC_RDMA] = 0;
	rdma_done[EX_VSYNC_RDMA] = true;
	spin_unlock_irqrestore(&lock, flags);
	irq_count[EX_VSYNC_RDMA]++;
}

/* add a register handle (including register addr) to read list
 * success: return  0
 *   fail:  return -1
 */
s32 VSYNC_ADD_RD_REG(struct reg_handle *reg_hnd)
{
	int enable_ = cur_enable[VSYNC_RDMA_READ] & 0xf;
	int handle = vsync_rdma_handle[VSYNC_RDMA_READ];

	if (reg_hnd && enable_ != 0 && handle > 0)
		return rdma_add_read_reg(handle, reg_hnd);

	pr_err("%s: VSYNC_RDMA_READ is disabled\n", __func__);
	return -1;
}
EXPORT_SYMBOL(VSYNC_ADD_RD_REG);

/* remove register(s) from read list
 * success: return 0
 *   fail:  return -1
 */
s32 VSYNC_REMOVE_RD_REG(struct reg_handle *reg_hnd, u32 count)
{
	int enable_ = cur_enable[VSYNC_RDMA_READ] & 0xf;
	int handle = vsync_rdma_handle[VSYNC_RDMA_READ];

	if (reg_hnd && enable_ != 0 && handle > 0)
		return rdma_remove_read_reg(handle, reg_hnd, count);

	pr_err("%s: VSYNC_RDMA_READ is disabled\n", __func__);
	return -1;
}
EXPORT_SYMBOL(VSYNC_REMOVE_RD_REG);

/* query reg_handle by index from read list
 * success: return reg_handle pointer
 *   fail:  return NULL
 */
struct reg_handle *VSYNC_QUERY_RD_HANDLE(u32 index)
{
	int enable_ = cur_enable[VSYNC_RDMA_READ] & 0xf;
	int handle = vsync_rdma_handle[VSYNC_RDMA_READ];

	if (enable_ != 0 && handle > 0)
		return rdma_query_read_handle(handle, index);

	pr_err("%s: VSYNC_RDMA_READ is disabled\n", __func__);
	return NULL;
}
EXPORT_SYMBOL(VSYNC_QUERY_RD_HANDLE);

/* get read-back addr, this func should be invoked everytime before getting vals
 * success: return addr of read-back
 *   fail: return NULL
 */
u32 *VSYNC_GET_RD_BACK_ADDR(struct reg_handle *reg_hnd)
{
	int enable_ = cur_enable[VSYNC_RDMA_READ] & 0xf;
	int handle = vsync_rdma_handle[VSYNC_RDMA_READ];

	if (reg_hnd && enable_ != 0 && handle > 0)
		return rdma_get_read_back_addr(handle, reg_hnd);

	return NULL;
}
EXPORT_SYMBOL(VSYNC_GET_RD_BACK_ADDR);

u32 VSYNC_RD_MPEG_REG(u32 adr)
{
	int enable_ = cur_enable[cur_vsync_handle_id] & 0xf;

	u32 read_val = Rd(adr);

	if (enable_ != 0 && vsync_rdma_handle[cur_vsync_handle_id] > 0)
		read_val = rdma_read_reg(vsync_rdma_handle[cur_vsync_handle_id], adr);

	return read_val;
}
EXPORT_SYMBOL(VSYNC_RD_MPEG_REG);

u32 VSYNC_RD_TABLE_REG(int tbl_idx, u32 adr)
{
	int enable_ = cur_enable[cur_vsync_handle_id] & 0xf;

	u32 read_val = Rd(adr);

	if (enable_ != 0 && vsync_rdma_handle[cur_vsync_handle_id] > 0) {
		if (cur_vsync_handle_id == VSYNC_RDMA) {
			if (get_part_flag_status(RDMA_VPP0, tbl_idx))
				read_val = rdma_part_read_reg(tbl_idx,
					vsync_rdma_handle[cur_vsync_handle_id], adr);
		} else if (cur_vsync_handle_id == EX_VSYNC_RDMA) {
			if (get_part_flag_status(RDMA_LOWLATENCY, tbl_idx))
				read_val = rdma_part_read_reg(tbl_idx,
					vsync_rdma_handle[cur_vsync_handle_id], adr);
		} else {
			read_val = rdma_read_reg(vsync_rdma_handle[cur_vsync_handle_id], adr);
		}
	}

	return read_val;
}
EXPORT_SYMBOL(VSYNC_RD_TABLE_REG);

u32 VSYNC_RD_MPEG_REG_VPP1(u32 adr)
{
	int enable_ = cur_enable[VSYNC_RDMA_VPP1] & 0xf;

	u32 read_val = Rd(adr);

	if (enable_ != 0 && vsync_rdma_handle[VSYNC_RDMA_VPP1] > 0)
		read_val = rdma_read_reg(vsync_rdma_handle[VSYNC_RDMA_VPP1], adr);

	return read_val;
}
EXPORT_SYMBOL(VSYNC_RD_MPEG_REG_VPP1);

u32 VSYNC_RD_MPEG_REG_VPP2(u32 adr)
{
	int enable_ = cur_enable[VSYNC_RDMA_VPP2] & 0xf;

	u32 read_val = Rd(adr);

	if (enable_ != 0 && vsync_rdma_handle[VSYNC_RDMA_VPP2] > 0)
		read_val = rdma_read_reg(vsync_rdma_handle[VSYNC_RDMA_VPP2], adr);

	return read_val;
}
EXPORT_SYMBOL(VSYNC_RD_MPEG_REG_VPP2);

u32 PRE_VSYNC_RD_MPEG_REG(u32 adr)
{
	int enable_ = cur_enable[PRE_VSYNC_RDMA] & 0xf;
	u32 read_val = Rd(adr);

	if (enable_ != 0 && vsync_rdma_handle[PRE_VSYNC_RDMA] > 0)
		read_val = rdma_read_reg(vsync_rdma_handle[PRE_VSYNC_RDMA], adr);
	return read_val;
}
EXPORT_SYMBOL(PRE_VSYNC_RD_MPEG_REG);

u32 PRE_VSYNC_RD_TABLE_REG(int tbl_idx, u32 adr)
{
	int enable_ = cur_enable[PRE_VSYNC_RDMA] & 0xf;
	u32 read_val = Rd(adr);

	if (enable_ != 0 && vsync_rdma_handle[PRE_VSYNC_RDMA] > 0) {
		if (get_part_flag_status(RDMA_PRE_VSYNC, tbl_idx))
			read_val = rdma_part_read_reg(tbl_idx,
						vsync_rdma_handle[PRE_VSYNC_RDMA], adr);
		else
			read_val = rdma_read_reg(vsync_rdma_handle[PRE_VSYNC_RDMA], adr);
	}
	return read_val;
}
EXPORT_SYMBOL(PRE_VSYNC_RD_TABLE_REG);

int VSYNC_WR_MPEG_REG(u32 adr, u32 val)
{
	int enable_ = cur_enable[cur_vsync_handle_id] & 0xf;

	if (enable_ != 0 && vsync_rdma_handle[cur_vsync_handle_id] > 0) {
		rdma_write_reg(vsync_rdma_handle[cur_vsync_handle_id], adr, val);
	} else {
		Wr(adr, val);
		if (debug_flag[cur_vsync_handle_id] & 1)
			pr_info("VSYNC_WR(%x)=%x\n", adr, val);
	}
	return 0;
}
EXPORT_SYMBOL(VSYNC_WR_MPEG_REG);

int VSYNC_WR_TABLE_REG(int tbl_idx, u32 adr, u32 val)
{
	int enable_ = cur_enable[cur_vsync_handle_id] & 0xf;

	if (enable_ != 0 && vsync_rdma_handle[cur_vsync_handle_id] > 0) {
		if (cur_vsync_handle_id == VSYNC_RDMA) {
			if (get_part_flag_status(RDMA_VPP0, tbl_idx))
				rdma_part_write_reg(tbl_idx,
						vsync_rdma_handle[cur_vsync_handle_id], adr, val);
		} else if (cur_vsync_handle_id == EX_VSYNC_RDMA) {
			if (get_part_flag_status(RDMA_LOWLATENCY, tbl_idx))
				rdma_part_write_reg(tbl_idx,
						vsync_rdma_handle[cur_vsync_handle_id], adr, val);
		} else {
			rdma_write_reg(vsync_rdma_handle[cur_vsync_handle_id], adr, val);
		}
	} else {
		Wr(adr, val);
		if (debug_flag[cur_vsync_handle_id] & 1)
			pr_info("VSYNC_WR(%x)=%x\n", adr, val);
	}
	return 0;
}
EXPORT_SYMBOL(VSYNC_WR_TABLE_REG);

int VSYNC_WR_MPEG_REG_VPP1(u32 adr, u32 val)
{
	int enable_ = cur_enable[VSYNC_RDMA_VPP1] & 0xf;

	if (enable_ != 0 && vsync_rdma_handle[VSYNC_RDMA_VPP1] > 0) {
		rdma_write_reg(vsync_rdma_handle[VSYNC_RDMA_VPP1], adr, val);
	} else {
		Wr(adr, val);
		if (debug_flag[VSYNC_RDMA_VPP1] & 1)
			pr_info("VSYNC_VPP1_WR(%x)=%x\n", adr, val);
	}
	return 0;
}
EXPORT_SYMBOL(VSYNC_WR_MPEG_REG_VPP1);

int VSYNC_WR_MPEG_REG_VPP2(u32 adr, u32 val)
{
	int enable_ = cur_enable[VSYNC_RDMA_VPP2] & 0xf;

	if (enable_ != 0 && vsync_rdma_handle[VSYNC_RDMA_VPP2] > 0) {
		rdma_write_reg(vsync_rdma_handle[VSYNC_RDMA_VPP2], adr, val);
	} else {
		Wr(adr, val);
		if (debug_flag[VSYNC_RDMA_VPP2] & 1)
			pr_info("VSYNC_VPP2_WR(%x)=%x\n", adr, val);
	}
	return 0;
}
EXPORT_SYMBOL(VSYNC_WR_MPEG_REG_VPP2);

int PRE_VSYNC_WR_MPEG_REG(u32 adr, u32 val)
{
	int enable_ = cur_enable[PRE_VSYNC_RDMA] & 0xf;

	if (enable_ != 0 && vsync_rdma_handle[PRE_VSYNC_RDMA] > 0) {
		rdma_write_reg(vsync_rdma_handle[PRE_VSYNC_RDMA], adr, val);
	} else {
		Wr(adr, val);
		if (debug_flag[PRE_VSYNC_RDMA] & 1)
			pr_info("PRE_VSYNC_RDMA_WR(%x)=%x\n", adr, val);
	}
	return 0;
}
EXPORT_SYMBOL(PRE_VSYNC_WR_MPEG_REG);

int PRE_VSYNC_WR_TABLE_REG(int tbl_idx, u32 adr, u32 val)
{
	int enable_ = cur_enable[PRE_VSYNC_RDMA] & 0xf;

	if (enable_ != 0 && vsync_rdma_handle[PRE_VSYNC_RDMA] > 0) {
		if (get_part_flag_status(RDMA_PRE_VSYNC, tbl_idx))
			rdma_part_write_reg(tbl_idx,
					vsync_rdma_handle[PRE_VSYNC_RDMA], adr, val);
		else
			rdma_write_reg(vsync_rdma_handle[PRE_VSYNC_RDMA], adr, val);
	} else {
		Wr(adr, val);
		if (debug_flag[PRE_VSYNC_RDMA] & 1)
			pr_info("PRE_VSYNC_RDMA_WR(%x)=%x\n", adr, val);
	}
	return 0;
}
EXPORT_SYMBOL(PRE_VSYNC_WR_TABLE_REG);

int VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
	int enable_ = cur_enable[cur_vsync_handle_id] & 0xf;

	if (enable_ != 0 && vsync_rdma_handle[cur_vsync_handle_id] > 0) {
		rdma_write_reg_bits(vsync_rdma_handle[cur_vsync_handle_id],
				    adr, val, start, len);
	} else {
		u32 read_val = Rd(adr);
		u32 write_val = (read_val &
				 ~(((1L << (len)) - 1) << (start)))
			| ((unsigned int)(val) << (start));
		Wr(adr, write_val);
		if (debug_flag[cur_vsync_handle_id] & 1)
			pr_info("VSYNC_WR(%x)=%x\n", adr, write_val);
	}
	return 0;
}
EXPORT_SYMBOL(VSYNC_WR_MPEG_REG_BITS);

int VSYNC_WR_TABLE_REG_BITS(int tbl_idx, u32 adr, u32 val, u32 start, u32 len)
{
	int enable_ = cur_enable[cur_vsync_handle_id] & 0xf;

	if (enable_ != 0 && vsync_rdma_handle[cur_vsync_handle_id] > 0) {
		if (cur_vsync_handle_id == VSYNC_RDMA) {
			if (get_part_flag_status(RDMA_VPP0, tbl_idx))
				rdma_part_write_reg_bits(tbl_idx,
					vsync_rdma_handle[cur_vsync_handle_id],
				adr, val, start, len);
		} else if (cur_vsync_handle_id == EX_VSYNC_RDMA) {
			if (get_part_flag_status(RDMA_LOWLATENCY, tbl_idx))
				rdma_part_write_reg_bits(tbl_idx,
					vsync_rdma_handle[cur_vsync_handle_id],
					adr, val, start, len);
		} else {
			rdma_write_reg_bits(vsync_rdma_handle[cur_vsync_handle_id],
					adr, val, start, len);
		}
	} else {
		u32 read_val = Rd(adr);
		u32 write_val = (read_val &
				 ~(((1L << (len)) - 1) << (start)))
			| ((unsigned int)(val) << (start));
		Wr(adr, write_val);
		if (debug_flag[cur_vsync_handle_id] & 1)
			pr_info("VSYNC_WR(%x)=%x\n", adr, write_val);
	}
	return 0;
}
EXPORT_SYMBOL(VSYNC_WR_TABLE_REG_BITS);

int VSYNC_WR_MPEG_REG_BITS_VPP1(u32 adr, u32 val, u32 start, u32 len)
{
	int enable_ = cur_enable[VSYNC_RDMA_VPP1] & 0xf;

	if (enable_ != 0 && vsync_rdma_handle[VSYNC_RDMA_VPP1] > 0) {
		rdma_write_reg_bits(vsync_rdma_handle[VSYNC_RDMA_VPP1],
				    adr, val, start, len);
	} else {
		u32 read_val = Rd(adr);
		u32 write_val = (read_val &
				 ~(((1L << (len)) - 1) << (start)))
			| ((unsigned int)(val) << (start));
		Wr(adr, write_val);
		if (debug_flag[VSYNC_RDMA_VPP1] & 1)
			pr_info("VSYNC_VPP1_WR(%x)=%x\n", adr, write_val);
	}
	return 0;
}
EXPORT_SYMBOL(VSYNC_WR_MPEG_REG_BITS_VPP1);

int VSYNC_WR_MPEG_REG_BITS_VPP2(u32 adr, u32 val, u32 start, u32 len)
{
	int enable_ = cur_enable[VSYNC_RDMA_VPP2] & 0xf;

	if (enable_ != 0 && vsync_rdma_handle[VSYNC_RDMA_VPP2] > 0) {
		rdma_write_reg_bits(vsync_rdma_handle[VSYNC_RDMA_VPP2],
				    adr, val, start, len);
	} else {
		u32 read_val = Rd(adr);
		u32 write_val = (read_val &
				 ~(((1L << (len)) - 1) << (start)))
			| ((unsigned int)(val) << (start));
		Wr(adr, write_val);
		if (debug_flag[VSYNC_RDMA_VPP2] & 1)
			pr_info("VSYNC_VPP2_WR(%x)=%x\n", adr, write_val);
	}
	return 0;
}
EXPORT_SYMBOL(VSYNC_WR_MPEG_REG_BITS_VPP2);

int PRE_VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
	int enable_ = cur_enable[PRE_VSYNC_RDMA] & 0xf;

	if (enable_ != 0 && vsync_rdma_handle[PRE_VSYNC_RDMA] > 0) {
		rdma_write_reg_bits(vsync_rdma_handle[PRE_VSYNC_RDMA],
				    adr, val, start, len);
	} else {
		u32 read_val = Rd(adr);
		u32 write_val = (read_val &
				 ~(((1L << (len)) - 1) << (start)))
			| ((unsigned int)(val) << (start));
		Wr(adr, write_val);
		if (debug_flag[PRE_VSYNC_RDMA] & 1)
			pr_info("PRE_VSYNC_VPP2_WR(%x)=%x\n", adr, write_val);
	}
	return 0;
}
EXPORT_SYMBOL(PRE_VSYNC_WR_MPEG_REG_BITS);

int PRE_VSYNC_WR_TABLE_REG_BITS(int tbl_idx, u32 adr, u32 val, u32 start, u32 len)
{
	int enable_ = cur_enable[PRE_VSYNC_RDMA] & 0xf;

	if (enable_ != 0 && vsync_rdma_handle[PRE_VSYNC_RDMA] > 0) {
		if (get_part_flag_status(RDMA_PRE_VSYNC, tbl_idx))
			rdma_part_write_reg_bits(tbl_idx, vsync_rdma_handle[PRE_VSYNC_RDMA],
					adr, val, start, len);
		else
			rdma_write_reg_bits(vsync_rdma_handle[PRE_VSYNC_RDMA],
				    adr, val, start, len);
	} else {
		u32 read_val = Rd(adr);
		u32 write_val = (read_val &
				 ~(((1L << (len)) - 1) << (start)))
			| ((unsigned int)(val) << (start));
		Wr(adr, write_val);
		if (debug_flag[PRE_VSYNC_RDMA] & 1)
			pr_info("PRE_VSYNC_VPP2_WR(%x)=%x\n", adr, write_val);
	}
	return 0;
}
EXPORT_SYMBOL(PRE_VSYNC_WR_TABLE_REG_BITS);

u32 _VSYNC_RD_MPEG_REG(u32 adr)
{
	u32 read_val = 0;

	int enable_ = cur_enable[LINE_N_INT_RDMA] & 0xf;

	read_val = Rd(adr);

	if (enable_ != 0 && vsync_rdma_handle[LINE_N_INT_RDMA] > 0)
		read_val = rdma_read_reg
			(vsync_rdma_handle[LINE_N_INT_RDMA], adr);

	return read_val;
}
EXPORT_SYMBOL(_VSYNC_RD_MPEG_REG);

int _VSYNC_WR_MPEG_REG(u32 adr, u32 val)
{
	int enable_ = cur_enable[LINE_N_INT_RDMA] & 0xf;

	if (enable_ != 0 &&
	    vsync_rdma_handle[LINE_N_INT_RDMA] > 0) {
		rdma_write_reg
			(vsync_rdma_handle[LINE_N_INT_RDMA], adr, val);
	} else {
		Wr(adr, val);
		if (debug_flag[LINE_N_INT_RDMA] & 1)
			pr_info("VSYNC_WR(%x)=%x\n", adr, val);
	}

	return 0;
}
EXPORT_SYMBOL(_VSYNC_WR_MPEG_REG);

int _VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
	int enable_ = cur_enable[LINE_N_INT_RDMA] & 0xf;

	if (enable_ != 0 &&
	    vsync_rdma_handle[LINE_N_INT_RDMA] > 0) {
		rdma_write_reg_bits
			(vsync_rdma_handle[LINE_N_INT_RDMA],
			adr, val, start, len);
	} else {
		u32 read_val = Rd(adr);
		u32 write_val = (read_val &
				 ~(((1L << (len)) - 1) << (start)))
			| ((unsigned int)(val) << (start));
		Wr(adr, write_val);
		if (debug_flag[LINE_N_INT_RDMA] & 1)
			pr_info("VSYNC_WR(%x)<=%x\n", adr, write_val);
	}
	return 0;
}
EXPORT_SYMBOL(_VSYNC_WR_MPEG_REG_BITS);

bool is_vsync_rdma_enable(void)
{
	bool ret;
	int enable_ = cur_enable[cur_vsync_handle_id] & 0xf;

	ret = (enable_ != 0);
	return ret;
}
EXPORT_SYMBOL(is_vsync_rdma_enable);

bool is_vsync_vpp1_rdma_enable(void)
{
	bool ret;
	int enable_ = cur_enable[VSYNC_RDMA_VPP1] & 0xf;

	ret = (enable_ != 0);
	return ret;
}
EXPORT_SYMBOL(is_vsync_vpp1_rdma_enable);

bool is_vsync_vpp2_rdma_enable(void)
{
	bool ret;
	int enable_ = cur_enable[VSYNC_RDMA_VPP2] & 0xf;

	ret = (enable_ != 0);
	return ret;
}
EXPORT_SYMBOL(is_vsync_vpp2_rdma_enable);

bool is_pre_vsync_rdma_enable(void)
{
	bool ret;
	int enable_ = cur_enable[PRE_VSYNC_RDMA] & 0xf;

	ret = (enable_ != 0);
	return ret;
}
EXPORT_SYMBOL(is_pre_vsync_rdma_enable);
void enable_rdma_log(int flag)
{
	if (flag) {
		debug_flag[VSYNC_RDMA] |= 0x1;
		debug_flag[EX_VSYNC_RDMA] |= 0x1;
		if (has_multi_vpp) {
			debug_flag[VSYNC_RDMA_VPP1] |= 0x1;
			debug_flag[VSYNC_RDMA_VPP2] |= 0x1;
			debug_flag[PRE_VSYNC_RDMA] |= 0x1;
			debug_flag[LINE_N_INT_RDMA] |= 0x1;
			if (read_rdma_en)
				debug_flag[VSYNC_RDMA_READ] |= 0x1;
		} else {
			debug_flag[LINE_N_INT_RDMA] |= 0x1;
			debug_flag[VSYNC_RDMA_READ] |= 0x1;
		}
	} else {
		debug_flag[VSYNC_RDMA] &= (~0x1);
		debug_flag[EX_VSYNC_RDMA] &= (~0x1);
		if (has_multi_vpp) {
			debug_flag[VSYNC_RDMA_VPP1] &= (~0x1);
			debug_flag[VSYNC_RDMA_VPP2] &= (~0x1);
			debug_flag[PRE_VSYNC_RDMA] &= (~0x1);
			debug_flag[LINE_N_INT_RDMA] &= (~0x1);
			if (read_rdma_en)
				debug_flag[VSYNC_RDMA_READ] &= (~0x1);
		} else {
			debug_flag[LINE_N_INT_RDMA] &= (~0x1);
			debug_flag[VSYNC_RDMA_READ] &= (~0x1);
		}
	}
}
EXPORT_SYMBOL(enable_rdma_log);

void enable_rdma(int enable_flag)
{
	enable[VSYNC_RDMA] = enable_flag;
	enable[EX_VSYNC_RDMA] = enable_flag;
	if (has_multi_vpp) {
		enable[VSYNC_RDMA_VPP1] = enable_flag;
		enable[VSYNC_RDMA_VPP2] = enable_flag;
		enable[PRE_VSYNC_RDMA] = enable_flag;
		enable[LINE_N_INT_RDMA] = enable_flag;
		if (read_rdma_en)
			enable[VSYNC_RDMA_READ] = enable_flag;
	} else {
		enable[LINE_N_INT_RDMA] = enable_flag;
		enable[VSYNC_RDMA_READ] = enable_flag;
	}
}
EXPORT_SYMBOL(enable_rdma);

int set_vsync_rdma_id(u8 id)
{
	if (!ex_vsync_rdma_enable) {
		cur_vsync_handle_id = VSYNC_RDMA;
		return (id != cur_vsync_handle_id) ? -1 : 0;
	}

	if (id != VSYNC_RDMA && id != EX_VSYNC_RDMA)
		return -1;
	cur_vsync_handle_id = id;
	return 0;
}
EXPORT_SYMBOL(set_vsync_rdma_id);

void set_rdma_channel_enable(u8 rdma_en)
{
	int i;

	for (i = 0; i < RDMA_NUM; i++)
		enable[i] = rdma_en;
}
EXPORT_SYMBOL(set_rdma_channel_enable);

void force_config_once(void)
{
	int i;

	for (i = 0; i < RDMA_NUM; i++)
		force_rdma_config[i] = 1;
}

struct rdma_op_s *get_rdma_ops(int rdma_type)
{
	if (has_multi_vpp) {
		if (rdma_type == VSYNC_RDMA)
			return &vsync_rdma_op;
		else if (rdma_type == VSYNC_RDMA_VPP1)
			return &vsync_rdma_vpp1_op;
		else if (rdma_type == VSYNC_RDMA_VPP2)
			return &vsync_rdma_vpp2_op;
		else if (rdma_type == PRE_VSYNC_RDMA)
			return &pre_vsync_rdma_op;
		else if (rdma_type == EX_VSYNC_RDMA)
			return &ex_vsync_rdma_op;
		else if (rdma_type == LINE_N_INT_RDMA)
			return &line_n_int_rdma_op;
		else if (read_rdma_en && rdma_type == VSYNC_RDMA_READ)
			return &vsync_rdma_read_op;
		else
			return NULL;
	} else {
		if (rdma_type == VSYNC_RDMA)
			return &vsync_rdma_op;
		else if (rdma_type == LINE_N_INT_RDMA)
			return &line_n_int_rdma_op;
		else if (rdma_type == VSYNC_RDMA_READ)
			return &vsync_rdma_read_op;
		else if (rdma_type == EX_VSYNC_RDMA)
			return &ex_vsync_rdma_op;
		else
			return NULL;
	}
}

void set_rdma_handle(int rdma_type, int handle)
{
	if (rdma_type < RDMA_NUM) {
		vsync_rdma_handle[rdma_type] = handle;
		pr_info("%s video rdma type:%d, handle = %d.\n", __func__, rdma_type,
			vsync_rdma_handle[rdma_type]);
	} else {
		pr_info("vsync_rdma_handle array overrun\n");
	}
}

void init_rdma_handle(void)
{
	int i;

	for (i = 0; i < RDMA_NUM; i++)
		vsync_rdma_handle[i] = -1;
}

int get_rdma_handle(int rdma_type)
{
	return vsync_rdma_handle[rdma_type];
}
EXPORT_SYMBOL(get_rdma_handle);

int get_rdma_type(int handle)
{
	int i, rdma_type = -1;

	for (i = 0; i < RDMA_NUM; i++) {
		if (handle == vsync_rdma_handle[i])
			rdma_type = i;
	}

	return rdma_type;
}

u32 is_line_n_rdma_enable(void)
{
	return second_rdma_feature;
}

static int parse_para(const char *para, int para_num, int *result)
{
	char *token = NULL;
	char *params, *params_base;
	int *out = result;
	int len = 0, count = 0;
	int res = 0;
	int ret = 0;

	if (!para)
		return 0;

	params = kstrdup(para, GFP_KERNEL);
	params_base = params;
	token = params;
	if (token) {
		len = strlen(token);
		do {
			token = strsep(&params, " ");
			if (!token)
				break;
			while (token &&
			       (isspace(*token) ||
				!isgraph(*token)) && len) {
				token++;
				len--;
			}
			if (len == 0)
				break;
			ret = kstrtoint(token, 0, &res);
			if (ret < 0)
				break;
			len = strlen(token);
			*out++ = res;
			count++;
		} while ((count < para_num) && (len > 0));
	}

	kfree(params_base);
	return count;
}

static ssize_t show_second_rdma_feature(const struct class *class,
					const struct class_attribute *attr,
					char *buf)
{
	return snprintf(buf, 40, "%d\n", second_rdma_feature);
}

static ssize_t store_second_rdma_feature(const struct class *class,
					 const struct class_attribute *attr,
					 const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	pr_info("second_rdma_feature: %d->%d\n", second_rdma_feature, res);
	second_rdma_feature = res;

	return count;
}

static ssize_t show_enable(const struct class *class,
			   const struct class_attribute *attr,
			   char *buf)
{
	int i;
	int enable_flag = 0;

	for (i = 0; i < RDMA_NUM; i++)
		if (enable[i])
			enable_flag |= (1 << i);

	return snprintf(buf, PAGE_SIZE, "enable_flag: 0x%x\n",
			enable_flag);
}

static ssize_t store_enable(const struct class *class,
			    const struct class_attribute *attr,
			    const char *buf, size_t count)
{
	int i = 0;
	int enable_flag = 0, ret;

	ret = kstrtoint(buf, 0, &enable_flag);
	for (i = 0; i < RDMA_NUM; i++) {
		enable[i] = (enable_flag >> i) & 1;
		pr_info("enable[%d]=%d\n", i, enable[i]);
	}
	return count;
}

static ssize_t show_irq_count(const struct class *class,
			      const struct class_attribute *attr,
			      char *buf)
{
	int i;
	char str[8];
	char buf_str[128] = {0};

	for (i = 0; i < RDMA_NUM; i++) {
		sprintf(str, "%d ", irq_count[i]);
		strcat(buf_str, str);
	}

	return snprintf(buf, PAGE_SIZE, "irq count: %s\n", buf_str);
}

static ssize_t store_irq_count(const struct class *class,
			       const struct class_attribute *attr,
			       const char *buf, size_t count)
{
	int i = 0;
	int channel = 0;
	int parsed[2];

	if (likely(parse_para(buf, 2, parsed) == 2)) {
		channel = parsed[0];
		if (channel < RDMA_NUM) {
			irq_count[channel] = parsed[1];
			pr_info("irq_count[%d]: %d\n", i, irq_count[i]);
		} else {
			pr_info("error please input: rdma_channel, irq_count\n");
		}
	} else {
		pr_info("error please input: rdma_channel, irq_count\n");
	}
	return count;
}

static ssize_t show_debug_flag(const struct class *class,
			       const struct class_attribute *attr,
			       char *buf)
{
	int i;
	char str[8];
	char buf_str[128] = {0};

	for (i = 0; i < RDMA_NUM; i++) {
		sprintf(str, "%d ", debug_flag[i]);
		strcat(buf_str, str);
	}

	return snprintf(buf, PAGE_SIZE, "debug_flag: %s\n", buf_str);
}

static ssize_t store_debug_flag(const struct class *class,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int channel = 0;
	int parsed[2];

	if (likely(parse_para(buf, 2, parsed) == 2)) {
		channel = parsed[0];
		if (channel < RDMA_NUM) {
			debug_flag[channel] = parsed[1];
			pr_info("debug_flag[%d]: %d\n", channel,
				debug_flag[channel]);
		} else {
			pr_info("error please input: rdma_channel, debug_flag\n");
		}
	} else {
		pr_info("error please input: rdma_channel, debug_flag\n");
	}
	return count;
}

static ssize_t show_vsync_cfg_count(const struct class *class,
				    const struct class_attribute *attr,
				    char *buf)
{
	int i;
	char str[8];
	char buf_str[128] = {0};

	for (i = 0; i < RDMA_NUM; i++) {
		sprintf(str, "%d ", vsync_cfg_count[i]);
		strcat(buf_str, str);
	}

	return snprintf(buf, PAGE_SIZE, "vsync_cfg_count: %s\n", buf_str);
}

static ssize_t store_vsync_cfg_count(const struct class *class,
				     const struct class_attribute *attr,
				     const char *buf, size_t count)
{
	int i = 0;
	int channel = 0;
	int parsed[2];

	if (likely(parse_para(buf, 2, parsed) == 2)) {
		channel = parsed[0];
		if (channel < RDMA_NUM) {
			vsync_cfg_count[channel] = parsed[1];
			pr_info("vsync_cfg_count[%d]: %d\n", i, vsync_cfg_count[i]);
		} else {
			pr_info("error please input: rdma_channel, vsync_cfg_count\n");
		}
	} else {
		pr_info("error please input: rdma_channel, debug_flag\n");
	}
	return count;
}

static ssize_t show_force_rdma_config(const struct class *class,
				      const struct class_attribute *attr,
				      char *buf)
{
	int i;
	char str[8];
	char buf_str[128] = {0};

	for (i = 0; i < RDMA_NUM; i++) {
		sprintf(str, "%d ", force_rdma_config[i]);
		strcat(buf_str, str);
	}

	return snprintf(buf, PAGE_SIZE, "force_rdma_config: %s\n", buf_str);
}

static ssize_t store_force_rdma_config(const struct class *class,
				       const struct class_attribute *attr,
				       const char *buf, size_t count)
{
	int rdma_type = VSYNC_RDMA;
	int parsed[2];

	if (likely(parse_para(buf, 2, parsed) == 2)) {
		rdma_type = parsed[0];
		if (rdma_type < RDMA_NUM) {
			force_rdma_config[rdma_type] = parsed[1];
			pr_info("force_rdma_config[%d]: %d\n",
				rdma_type, force_rdma_config[rdma_type]);
		} else {
			pr_info("error please input: rdma_type, force_rdma_config\n");
		}
	} else {
		pr_info("error please input: rdma_channel, force_rdma_config\n");
	}
	return count;
}

static ssize_t show_rdma_done_detect(const struct class *class,
				     const struct class_attribute *attr,
				     char *buf)
{
	return snprintf(buf, PAGE_SIZE,
			"use_rdma_done_detect:%d no_rdma_done_max_cnt:%d\n",
			use_rdma_done_detect, no_rdma_done_max_cnt);
}

static ssize_t store_rdma_done_detect(const struct class *class,
				      const struct class_attribute *attr,
				      const char *buf, size_t count)
{
	int parsed[2];

	if (likely(parse_para(buf, 2, parsed) == 2)) {
		use_rdma_done_detect = parsed[0];
		no_rdma_done_max_cnt = parsed[1];
		pr_info("use_rdma_done_detect:%d no_rdma_done_max_cnt:%d\n",
			use_rdma_done_detect, no_rdma_done_max_cnt);
	} else {
		pr_info("error please input: use_rdma_done_detect no_rdma_done_max_cnt\n");
	}

	return count;
}

static struct class_attribute rdma_attrs[] = {
	__ATTR(second_rdma_feature, 0664,
	       show_second_rdma_feature, store_second_rdma_feature),
	__ATTR(enable, 0664,
	       show_enable, store_enable),
	__ATTR(irq_count, 0664,
	       show_irq_count, store_irq_count),
	__ATTR(debug_flag, 0664,
	       show_debug_flag, store_debug_flag),
	__ATTR(vsync_cfg_count, 0664,
	       show_vsync_cfg_count, store_vsync_cfg_count),
	__ATTR(force_rdma_config, 0664,
	       show_force_rdma_config, store_force_rdma_config),
	__ATTR(rdma_done_detect, 0664,
	       show_rdma_done_detect, store_rdma_done_detect),
};

static struct class *rdma_class;
static int create_rdma_class(void)
{
	int i;

	rdma_class = class_create("rdma");
	if (IS_ERR_OR_NULL(rdma_class)) {
		pr_err("create rdma_class failed\n");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(rdma_attrs); i++) {
		if (class_create_file(rdma_class,
				      &rdma_attrs[i])) {
			pr_err("create rdma attribute %s failed\n",
			       rdma_attrs[i].attr.name);
		}
	}
	return 0;
}

static int remove_rdma_class(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rdma_attrs); i++)
		class_remove_file(rdma_class, &rdma_attrs[i]);

	class_destroy(rdma_class);
	rdma_class = NULL;
	return 0;
}

int rdma_init(void)

{
	if (is_meson_t3x_cpu()) {
		use_rdma_done_detect = 1;
		rdma_done_detect_reg = VIU_OSD4_MISC;
	}

	second_rdma_feature = 0;

	ex_vsync_rdma_enable = 1;
	cur_vsync_handle_id = VSYNC_RDMA;

	cur_enable[VSYNC_RDMA] = 0;
	enable[VSYNC_RDMA] = 1;
	force_rdma_config[VSYNC_RDMA] = 1;

	cur_enable[EX_VSYNC_RDMA] = 0;
	enable[EX_VSYNC_RDMA] = 1;
	force_rdma_config[EX_VSYNC_RDMA] = 1;

	if (has_multi_vpp) {
		cur_enable[VSYNC_RDMA_VPP1] = 0;
		enable[VSYNC_RDMA_VPP1] = 1;
		force_rdma_config[VSYNC_RDMA_VPP1] = 1;

		cur_enable[VSYNC_RDMA_VPP2] = 0;
		enable[VSYNC_RDMA_VPP2] = 1;
		force_rdma_config[VSYNC_RDMA_VPP2] = 1;
		cur_enable[PRE_VSYNC_RDMA] = 0;
		enable[PRE_VSYNC_RDMA] = 1;
		force_rdma_config[PRE_VSYNC_RDMA] = 1;

		cur_enable[LINE_N_INT_RDMA] = 0;
		enable[LINE_N_INT_RDMA] = 1;
		force_rdma_config[LINE_N_INT_RDMA] = 1;

		if (read_rdma_en) {
			cur_enable[VSYNC_RDMA_READ] = 0;
			enable[VSYNC_RDMA_READ] = 1;
			force_rdma_config[VSYNC_RDMA_READ] = 1;
		}
	} else {
		cur_enable[VSYNC_RDMA_READ] = 0;
		enable[VSYNC_RDMA_READ] = 1;
		force_rdma_config[VSYNC_RDMA_READ] = 1;

		if (second_rdma_feature) {
			cur_enable[LINE_N_INT_RDMA] = 0;
			enable[LINE_N_INT_RDMA] = 1;
			force_rdma_config[LINE_N_INT_RDMA] = 1;
		}
	}
	create_rdma_class();
	return 0;
}

void rdma_exit(void)
{
	remove_rdma_class();
}

