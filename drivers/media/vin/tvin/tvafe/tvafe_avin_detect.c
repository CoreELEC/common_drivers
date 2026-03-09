// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Standard Linux headers */
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include <linux/irq.h>
#include <linux/poll.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/of_address.h>

#include "tvafe_avin_detect.h"
#include "tvafe.h"
#include "tvafe_regs.h"
#include "tvafe_debug.h"
#include "../tvin_global.h"
#include "tvafe_general.h"

#define TVAFE_AVIN_CH1_MASK  BIT(0)
#define TVAFE_AVIN_CH2_MASK  BIT(1)
#define TVAFE_AVIN_MASK  (TVAFE_AVIN_CH1_MASK | TVAFE_AVIN_CH2_MASK)
#define TVAFE_MAX_AVIN_DEVICE_NUM  2
#define TVAFE_AVIN_NAME  "avin_detect"
#define TVAFE_AVIN_NAME_CH1  "tvafe_avin_detect_ch1"
#define TVAFE_AVIN_NAME_CH2  "tvafe_avin_detect_ch2"
#define AVIN_DETECT_CNT 40
#define AVIN_DETECT_SLEEP 3

static unsigned int avin_detect_debug_print;
unsigned int avin_detect_delay = AVIN_DETECT_DELAY;
unsigned int avin_detect_debug_val;

#define AVIN_DETECT_INIT          BIT(0)
#define AVIN_DETECT_OPEN          BIT(1)
#define AVIN_DETECT_CH1_MASK      BIT(2)
#define AVIN_DETECT_CH2_MASK      BIT(3)
#define AVIN_DETECT_CH1_EN        BIT(4)   /* 0x2e[0] */
#define AVIN_DETECT_CH1_SYNC_TIP  BIT(5)   /* 0x2e[3] */
#define AVIN_DETECT_CH2_EN        BIT(6)   /* 0x2e[15] */
#define AVIN_DETECT_CH2_SYNC_TIP  BIT(7)   /* 0x2e[18] */
static unsigned int avin_detect_flag;

#define AVIN_FUNCTION_WHITE0	  BIT(0)

/*0:use internal VDC to bias CVBS_in*/
/*1:use ground to bias CVBS_in*/
//static unsigned int detect_mode;

/*0:50mv; 1:100mv; 2:150mv; 3:200mv; 4:250mv; 6:300mv; 7:310mv*/
static unsigned int sync_level = 1;

//static unsigned int avplay_sync_level = 6;

/*0:50mv; 1:100mv; 2:150mv; 3:200mv*/
static unsigned int sync_hys_adj = 1;

static unsigned int irq_mode = 5;

static unsigned int trigger_sel = 1;

static unsigned int irq_edge_en = 1;

static unsigned int irq_pol;

static unsigned int avin_timer_time = 10;/*100ms*/

static bool detect_finish = true;
#define TVAFE_AVIN_INTERVAL (HZ / 100)/*10ms*/
static struct timer_list avin_detect_timer;
static unsigned int s_irq_counter0;
static unsigned int s_irq_counter1;

struct tvafe_avin_det_s *av_dev;
static struct meson_avin_data *meson_data;
struct avin_detect_state_s *detect_state;
struct work_struct     avin_read_reg_work;
struct workqueue_struct *avin_read_reg_wq;
#define AVIN_DETECT_CHARGE_TIME_MS	5

static DECLARE_WAIT_QUEUE_HEAD(tvafe_avin_waitq);

static unsigned int tvafe_avin_irq_reg_read(unsigned int reg)
{
	unsigned int val;

	if (meson_data->irq_reg_base)
		val = readl(meson_data->irq_reg_base + (reg << 2));
	else
		val = aml_read_cbus(reg);
	return val;
}

static unsigned int tvafe_avin_irq_reg_read_bit(u32 reg, const u32 start, const u32 len)
{
	u32 val;

	val = ((tvafe_avin_irq_reg_read(reg) >> (start)) & ((1L << (len)) - 1));

	return val;
}

static void tvafe_avin_irq_reg_write(unsigned int reg, unsigned int val)
{
	if (meson_data->irq_reg_base)
		writel(val, meson_data->irq_reg_base + (reg << 2));
	else
		aml_write_cbus(reg, val);
}

static void tvafe_avin_irq_update_bit(unsigned int reg,
			  unsigned int mask,
			  unsigned int val)
{
	unsigned int tmp, orig;

	if (meson_data->irq_reg_base) {
		orig = tvafe_avin_irq_reg_read(reg);

		tmp = orig & ~mask;
		tmp |= val & mask;
		writel(tmp, meson_data->irq_reg_base + (reg << 2));
	} else {
		aml_cbus_update_bits(reg, mask, val);
	}
}

static int tvafe_avin_dts_parse(struct platform_device *pdev)
{
	int ret, res;
	int i;
	int value;
	struct tvafe_avin_det_s *av_dev;

	av_dev = platform_get_drvdata(pdev);

	ret = of_property_read_u32(pdev->dev.of_node,
		"device_mask", &value);
	if (ret) {
		tvafe_pr_info("Failed to get device_mask.\n");
		goto get_avin_param_failed;
	} else {
		av_dev->dts_param.device_mask = value;
		tvafe_pr_info("avin device_mask is 0x%x\n",
			av_dev->dts_param.device_mask);
		if (av_dev->dts_param.device_mask == TVAFE_AVIN_MASK) {
			avin_detect_flag |=
				(AVIN_DETECT_CH1_MASK | AVIN_DETECT_CH2_MASK);
			av_dev->device_num = 2;
			ret = of_property_read_u32(pdev->dev.of_node,
				"device_sequence", &value);
			if (ret) {
				av_dev->dts_param.device_sequence = 0;
			} else {
				tvafe_pr_info("find device_sequence: %d\n",
					value);
				av_dev->dts_param.device_sequence = value;
			}
			if (av_dev->dts_param.device_sequence == 0) {
				av_dev->report_data_s[0].channel =
						TVAFE_AVIN_CHANNEL1;
				av_dev->report_data_s[1].channel =
						TVAFE_AVIN_CHANNEL2;
			} else {
				av_dev->report_data_s[0].channel =
						TVAFE_AVIN_CHANNEL2;
				av_dev->report_data_s[1].channel =
						TVAFE_AVIN_CHANNEL1;
			}
			tvafe_pr_info("avin ch1:%d, ch2:%d\n",
				av_dev->report_data_s[0].channel,
				av_dev->report_data_s[1].channel);
		} else if (av_dev->dts_param.device_mask ==
			TVAFE_AVIN_CH1_MASK) {
			avin_detect_flag |= AVIN_DETECT_CH1_MASK;
			av_dev->device_num = 1;
			av_dev->report_data_s[0].channel = TVAFE_AVIN_CHANNEL1;
			av_dev->report_data_s[1].channel =
				TVAFE_AVIN_CHANNEL_MAX;
		} else if (av_dev->dts_param.device_mask ==
			TVAFE_AVIN_CH2_MASK) {
			avin_detect_flag |= AVIN_DETECT_CH2_MASK;
			av_dev->device_num = 1;
			av_dev->report_data_s[0].channel = TVAFE_AVIN_CHANNEL2;
			av_dev->report_data_s[1].channel =
				TVAFE_AVIN_CHANNEL_MAX;
		} else {
			av_dev->device_num = 0;
			av_dev->report_data_s[0].channel =
				TVAFE_AVIN_CHANNEL_MAX;
			av_dev->report_data_s[1].channel =
				TVAFE_AVIN_CHANNEL_MAX;
			tvafe_pr_info("device_mask 0x%x invalid\n",
				av_dev->dts_param.device_mask);
			goto get_avin_param_failed;
		}
		av_dev->report_data_s[0].status = TVAFE_AVIN_STATUS_UNKNOWN;
		av_dev->report_data_s[1].status = TVAFE_AVIN_STATUS_UNKNOWN;
	}
	/* get irq no*/
	if (meson_data->detect_version == DETECTED_IRQ_VERSION) {
		for (i = 0; i < av_dev->device_num; i++) {
			res = platform_get_irq(pdev, i);
			if (!res) {
				tvafe_pr_err("%s: can't get avin(%d) irq resource\n",
					__func__, i);
				goto fail_get_resource_irq;
			}
			av_dev->dts_param.irq[i] = res;
		}
	}

	ret = of_property_read_u32(pdev->dev.of_node, "function_select", &value);
	if (!ret) {
		av_dev->function_select = value;
		tvafe_pr_info("function_select:0x%x\n", av_dev->function_select);
	}
	/* default open black field detect */
	av_dev->function_select |= AVIN_FUNCTION_WHITE0;

	return 0;
get_avin_param_failed:
fail_get_resource_irq:
	return -EINVAL;
}

void tvafe_avin_detect_ch1_anlog_enable(bool enable)
{
	if ((avin_detect_flag & AVIN_DETECT_OPEN) == 0) {
		if (avin_detect_debug_print & AVIN_NORMAL_DBG) {
			tvafe_pr_info("%s: avin_detect is not opened\n",
				__func__);
		}
		return;
	}
	if ((avin_detect_flag & AVIN_DETECT_CH1_MASK) == 0) {
		if (avin_detect_debug_print & AVIN_NORMAL_DBG) {
			tvafe_pr_info("%s: avin_detect ch1 is inactive\n",
				__func__);
		}
		return;
	}
	if (avin_detect_debug_print & AVIN_NORMAL_DBG)
		tvafe_pr_info("%s: %d\n", __func__, enable);

	/*txlx,txhd,tl1 the same bit:0 for ch1 en detect*/
	W_HIU_BIT(meson_data->detect_cntl, enable,
		AFE_CH1_EN_DETECT_BIT, AFE_CH1_EN_DETECT_WIDTH);

	if (enable)
		avin_detect_flag |= AVIN_DETECT_CH1_EN;
	else
		avin_detect_flag &= ~AVIN_DETECT_CH1_EN;
}

void tvafe_avin_detect_ch2_anlog_enable(bool enable)
{
	if ((avin_detect_flag & AVIN_DETECT_OPEN) == 0) {
		if (avin_detect_debug_print & AVIN_NORMAL_DBG) {
			tvafe_pr_info("%s: avin_detect is not opened\n",
				__func__);
		}
		return;
	}
	if ((avin_detect_flag & AVIN_DETECT_CH2_MASK) == 0) {
		if (avin_detect_debug_print & AVIN_NORMAL_DBG) {
			tvafe_pr_info("%s: avin_detect ch2 is inactive\n",
				__func__);
		}
		return;
	}

	W_HIU_BIT(meson_data->detect_cntl, enable,
		AFE_TL_CH2_EN_DETECT_BIT, AFE_TL_CH2_EN_DETECT_WIDTH);
	if (enable)
		avin_detect_flag |= AVIN_DETECT_CH2_EN;
	else
		avin_detect_flag &= ~AVIN_DETECT_CH2_EN;

	if (avin_detect_debug_print & AVIN_NORMAL_DBG)
		tvafe_pr_info("%s: %d\n", __func__, enable);
}

static void avin_detect_reset(void)
{
	W_HIU_BIT(meson_data->detect_cntl, 0,
		AFE_CH1_EN_DC_BIAS_BIT,
		AFE_CH1_EN_DC_BIAS_WIDTH);
	W_HIU_BIT(meson_data->detect_cntl, 0,
		AFE_CH1_EN_DETECT_BIT,
		AFE_CH1_EN_DETECT_WIDTH);

	W_HIU_BIT(meson_data->detect_cntl, 1,
		AFE_CH1_EN_DC_BIAS_BIT,
		AFE_CH1_EN_DC_BIAS_WIDTH);
	msleep(AVIN_DETECT_CHARGE_TIME_MS);
	W_HIU_BIT(meson_data->detect_cntl, 1,
		AFE_CH1_EN_DETECT_BIT,
		AFE_CH1_EN_DETECT_WIDTH);

	if (avin_detect_debug_print & AVIN_DETECT_T3X_DBG)
		tvafe_pr_info("detect reset\n");
}

static unsigned int avin_get_irq_counter(unsigned int irq_cnt)
{
	unsigned int val = 0;

	if (meson_data->detect_version == DETECTED_GPIO_VERSION)
		val = tvafe_avin_irq_reg_read_bit(irq_cnt,
			CVBS0_AFE_DIG_OUT_BIT, CVBS0_AFE_DIG_OUT_WIDTH);
	else
		val = tvafe_avin_irq_reg_read(irq_cnt);

	return val;
}

void avin_set_detect_status(bool on)
{
	W_HIU_BIT(meson_data->detect_cntl, on, AFE_CH1_EN_DETECT_BIT,
		AFE_CH1_EN_DETECT_WIDTH);
	W_HIU_BIT(meson_data->detect_cntl, on, AFE_CH1_EN_DC_BIAS_BIT,
		AFE_CH1_EN_DC_BIAS_WIDTH);
	W_APB_BIT(TVFE_CLAMP_INTF, !on, CLAMP_EN_BIT, CLAMP_EN_WID);
	if (avin_detect_debug_print & AVIN_DETECT_T3X_DBG)
		tvafe_pr_info("%s:%d\n", __func__, on);
}

//only for t3x
void tvafe_avin_detect_ch4(struct work_struct *work)
{
	int i, zero_cnt, one_cnt, val;
	bool early_ret = false;

	if (avport_opened) {
		if (detect_state->ch1_state == TVAFE_AVIN_STATUS_IN) {
			if (avin_detect_delay == 0 && !tvafe_signal_stable) {
				avin_set_detect_status(1);
			} else if (avin_detect_delay > 0) {
				avin_detect_delay--;
				if (avin_detect_debug_print & AVIN_DETECT_T3X_DBG)
					tvafe_pr_info("avin_detect_delay:%d\n",
					avin_detect_delay);
				early_ret = true;
			}
		}
	}
	if (early_ret)
		return;
	zero_cnt = 0;
	one_cnt = 0;
	detect_finish = false;
	for (i = 0; i < AVIN_DETECT_CNT; ++i) {
		val = avin_get_irq_counter(meson_data->irq0_cnt);
		if (val == 0)
			++zero_cnt;
		else
			++one_cnt;
		msleep(AVIN_DETECT_SLEEP);
	}

	if (detect_state->black_cnt < 2 && zero_cnt != AVIN_DETECT_CNT)
		detect_state->black_cnt = 0;

	if (zero_cnt == AVIN_DETECT_CNT) {
		detect_state->black_cnt++;
		if (avin_detect_debug_print & AVIN_DETECT_T3X_DBG)
			tvafe_pr_info("detect black\n");
	}

	if (detect_state->black_cnt > 0) {
		if (detect_state->black_cnt < 2) {
			avin_detect_reset();
			msleep(50);
		} else {
			if (avin_detect_debug_print & AVIN_DETECT_T3X_DBG)
				tvafe_pr_info("black signal\n");
			detect_state->ch1_state = TVAFE_AVIN_STATUS_IN;
			detect_state->black_cnt = 0;
		}
	} else if (one_cnt == av_dev->avin_detect_param.avin_out_count_times) {
		detect_state->ch1_state = TVAFE_AVIN_STATUS_OUT;
		detect_state->black_cnt = 0;
	} else if (zero_cnt > av_dev->avin_detect_param.avin_in_count_times) {
		detect_state->ch1_state = TVAFE_AVIN_STATUS_IN;
		detect_state->black_cnt = 0;
	} else {
		detect_state->black_cnt = 0;
	}

	detect_finish = true;
	if (avin_detect_debug_print & AVIN_DETECT_T3X_DBG)
		tvafe_pr_info("0 cnt:%d, 1 cnt:%d\n", zero_cnt, one_cnt);
}

/*After the CVBS is unplug,the EN_SYNC_TIP need be set to "1"*/
/*to sense the plug in operation*/
void tvafe_avin_detect_ch1_dc_enable(bool en)
{
	if ((avin_detect_flag & AVIN_DETECT_OPEN) == 0) {
		if (avin_detect_debug_print & AVIN_NORMAL_DBG) {
			tvafe_pr_info("%s: avin_detect is not opened\n",
				__func__);
		}
		return;
	}
	if ((avin_detect_flag & AVIN_DETECT_CH1_MASK) == 0) {
		if (avin_detect_debug_print & AVIN_NORMAL_DBG) {
			tvafe_pr_info("%s: avin_detect ch1 is inactive\n",
				__func__);
		}
		return;
	}

	W_HIU_BIT(meson_data->detect_cntl, en, AFE_CH1_EN_DC_BIAS_BIT,
		AFE_CH1_EN_DC_BIAS_WIDTH);
	avin_detect_flag |= AVIN_DETECT_CH1_SYNC_TIP;

	if (avin_detect_debug_print & AVIN_NORMAL_DBG)
		tvafe_pr_info("%s ok, en:%d\n", __func__, en);
}

void tvafe_avin_detect_ch2_dc_enable(bool en)
{
	if ((avin_detect_flag & AVIN_DETECT_OPEN) == 0) {
		if (avin_detect_debug_print & AVIN_NORMAL_DBG) {
			tvafe_pr_info("%s: avin_detect is not opened\n",
				__func__);
		}
		return;
	}
	if ((avin_detect_flag & AVIN_DETECT_CH2_MASK) == 0) {
		if (avin_detect_debug_print & AVIN_NORMAL_DBG) {
			tvafe_pr_info("%s: avin_detect ch2 is inactive\n",
				__func__);
		}
		return;
	}

	if (meson_data->cpu_id >= AVIN_CPU_TYPE_T5)
		W_HIU_BIT(meson_data->detect_cntl, en,
			AFE_T5_CH2_EN_DC_BIAS_BIT,
			AFE_T5_CH2_EN_DC_BIAS_WIDTH);
	else
		W_HIU_BIT(meson_data->detect_cntl, en,
			AFE_CH2_EN_DC_BIAS_BIT,
			AFE_CH2_EN_DC_BIAS_WIDTH);
	if (en)
		avin_detect_flag |= AVIN_DETECT_CH2_SYNC_TIP;
	else
		avin_detect_flag &= ~AVIN_DETECT_CH2_SYNC_TIP;

	if (avin_detect_debug_print & AVIN_NORMAL_DBG)
		tvafe_pr_info("%s ok, en:%d\n", __func__, en);
}

static void tvafe_avin_set_detect_en(struct tvafe_avin_det_s *avin_data, bool en)
{
	if ((avin_detect_flag & AVIN_DETECT_OPEN) == 0) {
		if (avin_detect_debug_print & AVIN_NORMAL_DBG) {
			tvafe_pr_info("%s: avin_detect is not opened\n",
				__func__);
		}
		return;
	}
	if (avin_data->dts_param.device_mask & TVAFE_AVIN_CH1_MASK)
		tvafe_avin_detect_ch1_anlog_enable(en);
	if (avin_data->dts_param.device_mask & TVAFE_AVIN_CH2_MASK)
		tvafe_avin_detect_ch2_anlog_enable(en);

	tvafe_pr_info("%s ok, en:%d\n", __func__, en);
}

static void tvafe_avin_detect_anlog_config(void)
{
		/*ch1 config*/
		W_HIU_BIT(meson_data->detect_cntl, meson_data->dc_level_adj,
			AFE_CH1_DC_LEVEL_ADJ_BIT, AFE_CH1_DC_LEVEL_ADJ_WIDTH);
		W_HIU_BIT(meson_data->detect_cntl, meson_data->comp_level_adj,
			AFE_CH1_COMP_LEVEL_ADJ_BIT, AFE_CH1_COMP_LEVEL_ADJ_WIDTH);
		W_HIU_BIT(meson_data->detect_cntl, 0,
			AFE_CH1_COMP_HYS_ADJ_BIT, AFE_CH1_COMP_HYS_ADJ_WIDTH);
		W_HIU_BIT(meson_data->detect_cntl, 1, AFE_CH1_EN_DC_BIAS_BIT,
			AFE_CH1_EN_DC_BIAS_WIDTH);

		if (meson_data->cpu_id >= AVIN_CPU_TYPE_T5) {
			W_HIU_BIT(meson_data->detect_cntl, 0,
				  AFE_T5_AFE_MAN_MODE_BIT,
				  AFE_T5_AFE_MAN_MODE_WIDTH);
			W_HIU_BIT(meson_data->detect_cntl, 1,
				  AFE_T5_CH2_EN_DC_BIAS_BIT,
				  AFE_T5_CH2_EN_DC_BIAS_WIDTH);
			W_HIU_BIT(meson_data->detect_cntl, meson_data->vdc_level,
				AFE_DETECT_RSV_BIT, AFE_DETECT_RSV_WIDTH);
		} else {
			/*ch config*/
			W_HIU_BIT(meson_data->detect_cntl, 1,
				  AFE_CH2_EN_DC_BIAS_BIT,
				  AFE_CH2_EN_DC_BIAS_WIDTH);
			W_HIU_BIT(meson_data->detect_cntl, 4,
				  AFE_CH2_DC_LEVEL_ADJ_BIT,
				  AFE_CH2_DC_LEVEL_ADJ_WIDTH);
			W_HIU_BIT(meson_data->detect_cntl, meson_data->comp_level_adj,
				  AFE_CH2_COMP_LEVEL_ADJ_BIT,
				  AFE_CH2_COMP_LEVEL_ADJ_WIDTH);
			W_HIU_BIT(meson_data->detect_cntl, 0,
				  AFE_CH2_COMP_HYS_ADJ_BIT,
				  AFE_CH2_COMP_HYS_ADJ_WIDTH);
		}

	avin_detect_flag |= AVIN_DETECT_CH2_SYNC_TIP;
	avin_detect_flag |= AVIN_DETECT_CH1_SYNC_TIP;
}

static void tvafe_avin_detect_digital_config(void)
{
	int i;
	unsigned int device_mask;
	unsigned int irq_cntl;

	if (!meson_data || !av_dev) {
		tvafe_pr_info("%s: meson_data or av_dev is null\n", __func__);
		return;
	}

	device_mask = av_dev->dts_param.device_mask;
	irq_cntl = meson_data->irq0_cntl;
	for (i = 0; i < TVAFE_MAX_AVIN_DEVICE_NUM && (device_mask & BIT(0)); i++) {
		if (meson_data->detect_version == DETECTED_IRQ_VERSION) {
			tvafe_avin_irq_update_bit(irq_cntl,
				CVBS_IRQ_MODE_MASK << CVBS_IRQ_MODE_BIT,
				irq_mode << CVBS_IRQ_MODE_BIT);
			tvafe_avin_irq_update_bit(irq_cntl,
				CVBS_IRQ_TRIGGER_SEL_MASK << CVBS_IRQ_TRIGGER_SEL_BIT,
				trigger_sel << CVBS_IRQ_TRIGGER_SEL_BIT);
			tvafe_avin_irq_update_bit(irq_cntl,
				CVBS_IRQ_EDGE_EN_MASK << CVBS_IRQ_EDGE_EN_BIT,
				irq_edge_en << CVBS_IRQ_EDGE_EN_BIT);
			tvafe_avin_irq_update_bit(irq_cntl,
				CVBS_IRQ_FILTER_MASK << CVBS_IRQ_FILTER_BIT,
				meson_data->irq_filter << CVBS_IRQ_FILTER_BIT);
			tvafe_avin_irq_update_bit(irq_cntl,
				CVBS_IRQ_POL_MASK << CVBS_IRQ_POL_BIT,
				irq_pol << CVBS_IRQ_POL_BIT);
		} else if (meson_data->detect_version == DETECTED_GPIO_VERSION) {
			if (av_dev->dts_param.device_mask & TVAFE_AVIN_CH1_MASK)
				tvafe_avin_irq_update_bit(irq_cntl,
					CVBS0_EN_DIG_FUNC_MASK << CVBS0_EN_DIG_FUNC_BIT,
					0x1 << CVBS0_EN_DIG_FUNC_BIT);
		} else {
			tvafe_pr_err("not confirm detected version");
		}
		device_mask >>= 1;
		irq_cntl = meson_data->irq1_cntl;
	}
}

static int tvafe_avin_open(struct inode *inode, struct file *file)
{
	struct tvafe_avin_det_s *avin_data;

	tvafe_pr_info("%s: avin open.\n", __func__);
	avin_data = container_of(inode->i_cdev,
		struct tvafe_avin_det_s, avin_cdev);
	file->private_data = avin_data;

	if (!avin_read_reg_wq) {
		avin_read_reg_wq =  create_singlethread_workqueue("t3x_avin_detect");
		INIT_WORK(&avin_read_reg_work, tvafe_avin_detect_ch4);
		if (!avin_read_reg_wq)
			return -ENOMEM;
	}
	tvafe_avin_init_resource();
	/*enable irq */
	avin_detect_flag |= AVIN_DETECT_OPEN;
	tvafe_avin_set_detect_en(avin_data, true);
	return 0;
}

static ssize_t tvafe_avin_read(struct file *file, char __user *buf,
	size_t count, loff_t *ppos)
{
	unsigned long ret;
	struct tvafe_avin_det_s *avin_data =
		(struct tvafe_avin_det_s *)file->private_data;

	if (!avin_data || avin_detect_flag == 0) {
		if (avin_detect_debug_print & AVIN_NORMAL_DBG)
			tvafe_pr_err("%s avin_data is null\n", __func__);
		return 0;
	}

	ret = copy_to_user(buf,
		(void *)(&avin_data->report_data_s[0]),
		sizeof(struct tvafe_report_data_s)
		* avin_data->device_num);

	return 0;
}

static int tvafe_avin_release(struct inode *inode, struct file *file)
{
	struct tvafe_avin_det_s *avin_data =
		(struct tvafe_avin_det_s *)file->private_data;

	tvafe_avin_set_detect_en(avin_data, false);
	del_timer_sync(&avin_detect_timer);
	cancel_work_sync(&avin_read_reg_work);
	avin_detect_flag &= ~AVIN_DETECT_OPEN;
	file->private_data = NULL;
	tvafe_pr_info("%s: avin release.\n", __func__);
	return 0;
}

static unsigned int tvafe_avin_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(file, &tvafe_avin_waitq, wait);
	mask |= POLLIN | POLLRDNORM;

	return mask;
}

static const struct file_operations tvafe_avin_fops = {
	.owner      = THIS_MODULE,
	.open       = tvafe_avin_open,
	.read       = tvafe_avin_read,
	.poll       = tvafe_avin_poll,
	.release    = tvafe_avin_release,
};

static int tvafe_register_avin_dev(struct tvafe_avin_det_s *avin_data)
{
	int ret = 0;

	ret = alloc_chrdev_region(&avin_data->avin_devno,
		0, 1, TVAFE_AVIN_NAME);
	if (ret < 0) {
		tvafe_pr_err("failed to allocate major number\n");
		return -ENODEV;
	}

	/* connect the file operations with cdev */
	cdev_init(&avin_data->avin_cdev, &tvafe_avin_fops);
	avin_data->avin_cdev.owner = THIS_MODULE;
	/* connect the major/minor number to the cdev */
	ret = cdev_add(&avin_data->avin_cdev, avin_data->avin_devno, 1);
	if (ret) {
		tvafe_pr_err("failed to add device\n");
		unregister_chrdev_region(avin_data->avin_devno, 1);
		return -ENODEV;
	}

	strcpy(avin_data->config_name, TVAFE_AVIN_NAME);
	avin_data->config_class = class_create(avin_data->config_name);
	avin_data->config_dev = device_create(avin_data->config_class, NULL,
		avin_data->avin_devno, NULL, avin_data->config_name);
	if (IS_ERR(avin_data->config_dev)) {
		tvafe_pr_err("failed to create device node\n");
		cdev_del(&avin_data->avin_cdev);
		unregister_chrdev_region(avin_data->avin_devno, 1);
		ret = PTR_ERR(avin_data->config_dev);
		return ret;
	}

	return ret;
}

static void tvafe_avin_detect_state(struct tvafe_avin_det_s *av_dev)
{
	tvafe_pr_info("device_num: %d\n", av_dev->device_num);
	tvafe_pr_info("\t*****dts param*****\n");
	tvafe_pr_info("device_mask: %d\n", av_dev->dts_param.device_mask);
	tvafe_pr_info("function_select: %#x\n", av_dev->function_select);
	tvafe_pr_info("irq0: %d\n", av_dev->dts_param.irq[0]);
	tvafe_pr_info("irq1: %d\n", av_dev->dts_param.irq[1]);
	tvafe_pr_info("irq_counter[0]: 0x%x\n", av_dev->irq_counter[0]);
	tvafe_pr_info("irq_counter[1]: 0x%x\n", av_dev->irq_counter[1]);
	tvafe_pr_info("\t*****channel status:0->in;1->out*****\n");
	tvafe_pr_info("channel[%d] status: %d\n",
		av_dev->report_data_s[0].channel,
		av_dev->report_data_s[0].status);
	tvafe_pr_info("channel[%d] status: %d\n",
		av_dev->report_data_s[1].channel,
		av_dev->report_data_s[1].status);
	tvafe_pr_info("avin_detect_flag: 0x%02x\n", avin_detect_flag);
	tvafe_pr_info("avin_detect_reg:  0x%02x=0x%08x\n",
		meson_data->detect_cntl, R_HIU_REG(meson_data->detect_cntl));
	tvafe_pr_info("avin_irq_reg:  0x%02x=0x%08x\n",
		meson_data->irq0_cntl,
		tvafe_avin_irq_reg_read(meson_data->irq0_cntl));
	tvafe_pr_info("\t*****global param*****\n");
	tvafe_pr_info("dc_level_adj: %d\n", meson_data->dc_level_adj);
	tvafe_pr_info("comp_level_adj: %d\n", meson_data->comp_level_adj);
	tvafe_pr_info("vdc_level: %d\n", meson_data->vdc_level);
	tvafe_pr_info("sync_level: %d\n", sync_level);
	tvafe_pr_info("sync_hys_adj: %d\n", sync_hys_adj);
	tvafe_pr_info("irq_mode: %d\n", irq_mode);
	tvafe_pr_info("trigger_sel: %d\n", trigger_sel);
	tvafe_pr_info("irq_edge_en: %d\n", irq_edge_en);
	tvafe_pr_info("irq_filter: %d\n", meson_data->irq_filter);
	tvafe_pr_info("irq_pol: %d\n", irq_pol);
	tvafe_pr_info("clamp:0x%x\n", R_APB_BIT(TVFE_CLAMP_INTF, 15, 1));
	tvafe_pr_info("avport_opened:%d\n", avport_opened);
	tvafe_pr_info("detect_finish:%d\n", detect_finish);
	tvafe_pr_info("ch1_plug_state:%d\n", detect_state->ch1_state);
	tvafe_pr_info("ch2_plug_state:%d\n", detect_state->ch2_state);
	tvafe_pr_info("ch1_dc_bias:%d\n",
		R_HIU_BIT(meson_data->detect_cntl, AFE_CH1_EN_DC_BIAS_BIT,
		AFE_CH1_EN_DC_BIAS_WIDTH));
	tvafe_pr_info("ch2_dc_bias:%d\n",
		R_HIU_BIT(meson_data->detect_cntl, AFE_CH2_EN_DC_BIAS_BIT,
		AFE_CH2_EN_DC_BIAS_WIDTH));
	tvafe_pr_info("detect_black:%d\n", detect_state->black_cnt);
	tvafe_pr_info("tvafe_start_flag:%d\n", tvafe_start_flag);
	tvafe_pr_info("avin_detect_delay:%d\n", avin_detect_delay);
	tvafe_pr_info("avin_detect_flag:%d\n", avin_detect_flag);
	tvafe_pr_info("av_detect_ver:%s\n", AV_DETECT_VER);
}

static void tvafe_avin_reg_print(void)
{
	tvafe_pr_info("avin_detect_cntl:  0x%02x=0x%08x\n",
		meson_data->detect_cntl, R_HIU_REG(meson_data->detect_cntl));
	tvafe_pr_info("avin_irq0_cntl:  0x%02x=0x%08x\n",
		meson_data->irq0_cntl,
		tvafe_avin_irq_reg_read(meson_data->irq0_cntl));
	tvafe_pr_info("avin_irq1_cntl:  0x%02x=0x%08x\n",
		meson_data->irq1_cntl,
		tvafe_avin_irq_reg_read(meson_data->irq1_cntl));
	tvafe_pr_info("avin_irq0_cnt:  0x%02x=0x%08x\n",
		meson_data->irq0_cnt,
		tvafe_avin_irq_reg_read(meson_data->irq0_cnt));
	tvafe_pr_info("avin_irq1_cnt:  0x%02x=0x%08x\n",
		meson_data->irq1_cnt,
		tvafe_avin_irq_reg_read(meson_data->irq1_cnt));
}

static void tvafe_avin_detect_parse_param(char *buf_orig, char **parm)
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

static ssize_t debug_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf + len,
		"\t*****usage:*****\n");
	len += sprintf(buf + len,
		"echo dc_level_adj val(D) > debug\n");
	len += sprintf(buf + len,
		"echo comp_level_adj val(D) > debug\n");
	len += sprintf(buf + len,
		"echo detect_mode val(D) > debug\n");
	len += sprintf(buf + len,
		"echo vdc_level val(D) > debug\n");
	len += sprintf(buf + len,
		"echo sync_level val(D) > debug\n");
	len += sprintf(buf + len,
		"echo sync_hys_adj val(D) > debug\n");
	len += sprintf(buf + len,
		"echo irq_mode val(D) > debug\n");
	len += sprintf(buf + len,
		"echo trigger_sel val(D) > debug\n");
	len += sprintf(buf + len,
		"echo irq_edge_en val(D) > debug\n");
	len += sprintf(buf + len,
		"echo irq_filter val(D) > debug\n");
	len += sprintf(buf + len,
		"echo irq_pol val(D) > debug\n");
	len += sprintf(buf + len,
		"echo ch1_enable val(D) > debug\n");
	len += sprintf(buf + len,
		"echo ch2_enable val(D) > debug\n");
	len += sprintf(buf + len,
		"echo enable > debug\n");
	len += sprintf(buf + len,
		"echo disable > debug\n");
	len += sprintf(buf + len,
		"echo status > debug\n");
	return len;
}

static ssize_t debug_store(struct device *dev,
	struct device_attribute *attr, const char *buff, size_t count)
{
	struct tvafe_avin_det_s *av_dev;
	unsigned int val;
	char *buf_orig, *parm[10] = {NULL};

	av_dev = dev_get_drvdata(dev);
	if (!buff)
		return count;
	buf_orig = kstrdup(buff, GFP_KERNEL);
	tvafe_avin_detect_parse_param(buf_orig, (char **)&parm);

	/*tvafe_pr_info("[%s]:param0:%s.\n", __func__, parm[0]);*/
	if (!strcmp(parm[0], "dc_level_adj")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &meson_data->dc_level_adj)) {
				tvafe_pr_info("[%s]:invalid parameter\n",
					__func__);
				goto tvafe_avin_detect_store_err;
			} else {
				W_HIU_BIT(meson_data->detect_cntl, meson_data->dc_level_adj,
					  AFE_CH1_DC_LEVEL_ADJ_BIT, AFE_CH1_DC_LEVEL_ADJ_WIDTH);
			}
		}
		tvafe_pr_info("[%s]: dc_level_adj: %d\n", __func__, meson_data->dc_level_adj);
	} else if (!strcmp(parm[0], "comp_level_adj")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &meson_data->comp_level_adj)) {
				tvafe_pr_info("[%s]:invalid parameter\n",
					__func__);
				goto tvafe_avin_detect_store_err;
			} else {
				W_HIU_BIT(meson_data->detect_cntl, meson_data->comp_level_adj,
					  AFE_CH1_COMP_LEVEL_ADJ_BIT,
					  AFE_CH1_COMP_LEVEL_ADJ_WIDTH);
			}
		}
		tvafe_pr_info("[%s]: comp_level_adj: %d\n", __func__, meson_data->comp_level_adj);
	} else if (!strcmp(parm[0], "vdc_level")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &meson_data->vdc_level)) {
				tvafe_pr_info("[%s]:invalid parameter\n",
					__func__);
				goto tvafe_avin_detect_store_err;
			}
		}
		tvafe_pr_info("[%s]: vdc_level: %d\n",
			__func__, meson_data->vdc_level);
	} else if (!strcmp(parm[0], "sync_level")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &sync_level)) {
				tvafe_pr_info("[%s]:invalid parameter\n",
					__func__);
				goto tvafe_avin_detect_store_err;
			}
		}
		tvafe_pr_info("[%s]: sync_level: %d\n",
			__func__, sync_level);
	} else if (!strcmp(parm[0], "sync_hys_adj")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &sync_hys_adj)) {
				tvafe_pr_info("[%s]:invalid parameter\n",
					__func__);
				goto tvafe_avin_detect_store_err;
			}
		}
		tvafe_pr_info("[%s]: sync_hys_adj: %d\n",
			__func__, sync_hys_adj);
	} else if (!strcmp(parm[0], "irq_mode")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &irq_mode)) {
				tvafe_pr_info("[%s]:invalid parameter\n",
					__func__);
				goto tvafe_avin_detect_store_err;
			}
		}
		tvafe_pr_info("[%s]: irq_mode: %d\n",
			__func__, irq_mode);
	} else if (!strcmp(parm[0], "trigger_sel")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &trigger_sel)) {
				tvafe_pr_info("[%s]:invalid parameter\n",
					__func__);
				goto tvafe_avin_detect_store_err;
			}
		}
		tvafe_pr_info("[%s]: trigger_sel: %d\n",
			__func__, trigger_sel);
	} else if (!strcmp(parm[0], "irq_edge_en")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &irq_edge_en)) {
				tvafe_pr_info("[%s]:invalid parameter\n",
					__func__);
				goto tvafe_avin_detect_store_err;
			}
		}
		tvafe_pr_info("[%s]: irq_edge_en: %d\n",
			__func__, irq_edge_en);
	} else if (!strcmp(parm[0], "irq_filter")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &meson_data->irq_filter)) {
				tvafe_pr_info("[%s]:invalid parameter\n",
					__func__);
				goto tvafe_avin_detect_store_err;
			}
		}
		tvafe_pr_info("[%s]: irq_filter: %d\n",
			__func__, meson_data->irq_filter);
	} else if (!strcmp(parm[0], "irq_pol")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &irq_pol)) {
				tvafe_pr_info("[%s]:invalid parameter\n",
					__func__);
				goto tvafe_avin_detect_store_err;
			}
		}
		tvafe_pr_info("[%s]: irq_pol: %d\n",
			__func__, irq_pol);
	} else if (!strcmp(parm[0], "ch1_enable")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &val)) {
				tvafe_pr_info("[%s]:invalid parameter\n",
					__func__);
				goto tvafe_avin_detect_store_err;
			}
			tvafe_pr_info("[%s]: ch1_enable: %d\n",
				__func__, val);
			if (val)
				tvafe_avin_detect_ch1_anlog_enable(1);
			else
				tvafe_avin_detect_ch1_anlog_enable(0);
		}
	} else if (!strcmp(parm[0], "ch2_enable")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &val)) {
				tvafe_pr_info("[%s]:invalid parameter\n",
					__func__);
				goto tvafe_avin_detect_store_err;
			}
			tvafe_pr_info("[%s]: ch2_enable: %d\n",
				__func__, val);
			if (val)
				tvafe_avin_detect_ch2_anlog_enable(1);
			else
				tvafe_avin_detect_ch2_anlog_enable(0);
		}
	} else if (!strcmp(parm[0], "enable")) {
		avin_detect_flag |= AVIN_DETECT_OPEN;
		tvafe_avin_set_detect_en(av_dev, true);
	} else if (!strcmp(parm[0], "disable")) {
		tvafe_avin_set_detect_en(av_dev, false);
		avin_detect_flag &= ~AVIN_DETECT_OPEN;
		av_dev->report_data_s[0].status = TVAFE_AVIN_STATUS_UNKNOWN;
		av_dev->report_data_s[1].status = TVAFE_AVIN_STATUS_UNKNOWN;
	} else if (!strcmp(parm[0], "status")) {
		tvafe_avin_detect_state(av_dev);
	} else if (!strcmp(parm[0], "print")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 16, &avin_detect_debug_print)) {
				tvafe_pr_info("[%s]:invalid parameter\n",
					__func__);
				goto tvafe_avin_detect_store_err;
			}
		}
		tvafe_pr_info("[%s]: avin_detect_debug_print: %d\n",
			__func__, avin_detect_debug_print);
	} else if (!strcmp(parm[0], "dbg_val")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &avin_detect_debug_val)) {
				tvafe_pr_info("[%s]:invalid parameter\n",
					__func__);
				goto tvafe_avin_detect_store_err;
			}
		}
		tvafe_pr_info("[%s]: avin_detect_debug_val: %d\n",
			__func__, avin_detect_debug_val);
	} else if (!strcmp(parm[0], "delay")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &avin_detect_delay)) {
				tvafe_pr_info("[%s]:invalid parameter\n",
					__func__);
				goto tvafe_avin_detect_store_err;
			}
		}
		tvafe_pr_info("[%s]: avin_detect_delay: %d\n",
			__func__, avin_detect_delay);
	} else if (!strcmp(parm[0], "reg_print")) {
		tvafe_avin_reg_print();
	} else if (!strcmp(parm[0], "w")) {
		if (!parm[1] || !parm[2]) {
			tvafe_pr_err("syntax error.\n");
		} else {
			if (kstrtouint(parm[2], 16, &val)) {
				tvafe_pr_info("[%s]:invalid parameter\n",
						__func__);
				goto tvafe_avin_detect_store_err;
			}

			if (!strcmp(parm[1], "detect_cntl"))
				W_HIU_REG(meson_data->detect_cntl, val);
			else if (!strcmp(parm[1], "irq0_cntl"))
				tvafe_avin_irq_reg_write(meson_data->irq0_cntl, val);
			else if (!strcmp(parm[1], "irq1_cntl"))
				tvafe_avin_irq_reg_write(meson_data->irq1_cntl, val);
			else
				tvafe_pr_err("command error\n");

			tvafe_pr_info("write done 0x%x\n", val);
		}
	} else if (!strcmp(parm[0], "function_select")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 16, &av_dev->function_select)) {
				tvafe_pr_info("[%s]:invalid parameter\n", __func__);
				goto tvafe_avin_detect_store_err;
			}
			tvafe_pr_info("[%s]: function_select:0x%x\n",
					__func__, av_dev->function_select);
		}
	} else {
		tvafe_pr_info("[%s]:invalid command.\n", __func__);
	}

	kfree(buf_orig);
	return count;

tvafe_avin_detect_store_err:
	kfree(buf_orig);
	return -EINVAL;
}

static bool tvafe_avin_detected_data(unsigned int irq_counter, unsigned int s_irq_counter)
{
	if (meson_data->detect_version == DETECTED_IRQ_VERSION) {
		if (irq_counter != s_irq_counter)
			return true;
	} else if (meson_data->detect_version == DETECTED_GPIO_VERSION) {
		if (detect_state->ch1_state == TVAFE_AVIN_STATUS_IN)
			return true;
		else if (detect_state->ch1_state == TVAFE_AVIN_STATUS_OUT)
			return false;
	} else {
		tvafe_pr_err("%s: detected version error\n", __func__);
	}

	return false;
}

static bool tvafe_avin_gpio_detect(void)
{
	bool ret = true;

	if (tvafe_start_flag)
		ret = false;
	else if (detect_finish)
		queue_work(avin_read_reg_wq, &avin_read_reg_work);
	else
		ret = false;

	if (detect_state->black_cnt > 0 && detect_state->black_cnt < 2)
		ret = false;

	return ret;
}

static void tvafe_avin_get_plug_sts(struct avin_detect_param_s *avin_detect_param)
{
	if (meson_data->detect_version == DETECTED_GPIO_VERSION)
		return;

	if (av_dev->dts_param.device_mask & TVAFE_AVIN_CH1_MASK) {
		if (avin_detect_param->s_irq_in_counter0_time >=
			avin_detect_param->avin_in_count_times)
			detect_state->ch1_state = TVAFE_AVIN_STATUS_IN;
		else if (avin_detect_param->s_irq_out_counter0_time >=
			avin_detect_param->avin_out_count_times)
			detect_state->ch1_state = TVAFE_AVIN_STATUS_OUT;
	}
	if (av_dev->dts_param.device_mask & TVAFE_AVIN_CH2_MASK) {
		if (avin_detect_param->s_irq_in_counter1_time >=
			avin_detect_param->avin_in_count_times)
			detect_state->ch2_state = TVAFE_AVIN_STATUS_IN;
		else if (avin_detect_param->s_irq_out_counter1_time >=
			avin_detect_param->avin_out_count_times)
			detect_state->ch2_state = TVAFE_AVIN_STATUS_OUT;
	}
}

static bool tvafe_avin_detect_ch1(struct avin_detect_param_s *avin_detect_param)
{
	bool state_changed = false;
	struct tvafe_dev_s *devp = tvafe_get_dev();

	if (tvafe_avin_detected_data(av_dev->irq_counter[0], s_irq_counter0)) {
		avin_detect_param->s_irq_in_counter0_time++;
		avin_detect_param->s_irq_out_counter0_time = 0;
		if (detect_state->ch1_state == TVAFE_AVIN_STATUS_IN) {
			if (av_dev->report_data_s[0].status != TVAFE_AVIN_STATUS_IN) {
				av_dev->report_data_s[0].status = TVAFE_AVIN_STATUS_IN;
				state_changed = true;
				avin_detect_delay = AVIN_DETECT_DELAY;
				if (avport_opened & 0x3)
					tvafe_reset_module();
				tvafe_pr_info("avin[0].status IN.\n");
				if ((avport_opened & 0x03) == TVAFE_PORT_AV1) {
					W_APB_BIT(TVFE_CLAMP_INTF, 1, CLAMP_EN_BIT, CLAMP_EN_WID);
					tvafe_avin_detect_ch1_dc_enable(false);
					tvafe_try_format(&devp->tvafe.cvd2, &devp->mem,
						TVIN_SIG_FMT_CVBS_PAL_I);
				}
			}
			avin_detect_param->s_irq_in_counter0_time = 0;
		}
	} else {
		avin_detect_param->s_irq_out_counter0_time++;
		if ((avport_opened & 0x03) == TVAFE_PORT_AV1 &&
		    (avin_detect_debug_print & AVIN_SIGNAL_DBG))
			tvafe_pr_info("0x3a:0x%x\n", R_APB_REG(CVD2_STATUS_REGISTER1));
		if (detect_state->ch1_state == TVAFE_AVIN_STATUS_OUT) {
			if (av_dev->report_data_s[0].status != TVAFE_AVIN_STATUS_OUT) {
				av_dev->report_data_s[0].status = TVAFE_AVIN_STATUS_OUT;
				state_changed = true;
				tvafe_pr_info("avin[0].status OUT.\n");
				if ((avport_opened & 0x03) == TVAFE_PORT_AV1) {
					W_APB_BIT(TVFE_CLAMP_INTF, 0, CLAMP_EN_BIT, CLAMP_EN_WID);
					tvafe_avin_detect_ch1_dc_enable(true);
				}
			}
			avin_detect_param->s_irq_out_counter0_time = 0;
			avin_detect_param->s_irq_in_counter0_time = 0;
		}
	}
	s_irq_counter0 = av_dev->irq_counter[0];
	return state_changed;
}

static bool tvafe_avin_detect_ch2(struct avin_detect_param_s *avin_detect_param)
{
	bool state_changed = false;
	struct tvafe_dev_s *devp = tvafe_get_dev();

	if (tvafe_avin_detected_data(av_dev->irq_counter[1], s_irq_counter1)) {
		avin_detect_param->s_irq_in_counter1_time++;
		avin_detect_param->s_irq_out_counter1_time = 0;
		if (detect_state->ch2_state == TVAFE_AVIN_STATUS_IN) {
			if (av_dev->report_data_s[1].status != TVAFE_AVIN_STATUS_IN) {
				av_dev->report_data_s[1].status = TVAFE_AVIN_STATUS_IN;
				state_changed = true;
				if (avport_opened & 0x3)
					tvafe_reset_module();
				tvafe_pr_info("avin[1].status IN.\n");
				/*port opened and plug in,enable clamp*/
				/*sync tip close*/
				if ((avport_opened & 0x3) == TVAFE_PORT_AV2) {
					W_APB_BIT(TVFE_CLAMP_INTF, 1, CLAMP_EN_BIT, CLAMP_EN_WID);
					tvafe_avin_detect_ch2_dc_enable(false);
					tvafe_try_format(&devp->tvafe.cvd2, &devp->mem,
						TVIN_SIG_FMT_CVBS_PAL_I);
				}
			}
			avin_detect_param->s_irq_in_counter1_time = 0;
			avin_detect_param->s_irq_out_counter1_time = 0;
		}
	} else {
		avin_detect_param->s_irq_out_counter1_time++;
		//if ((av_dev->function_select & AVIN_FUNCTION_WHITE0) &&
		//	tvafe_clk_status && (avport_opened & 0x03) == TVAFE_PORT_AV2 &&
		//	!R_APB_BIT(CVD2_STATUS_REGISTER1, NO_SIGNAL_BIT, NO_SIGNAL_WID))
		//	avin_detect_param->s_irq_out_counter1_time = 0;
		if ((avport_opened & 0x03) == TVAFE_PORT_AV2 &&
			(avin_detect_debug_print & AVIN_SIGNAL_DBG))
			tvafe_pr_info("0x3a:0x%x\n", R_APB_REG(CVD2_STATUS_REGISTER1));
		if (detect_state->ch1_state == TVAFE_AVIN_STATUS_OUT) {
			if (av_dev->report_data_s[1].status != TVAFE_AVIN_STATUS_OUT) {
				av_dev->report_data_s[1].status = TVAFE_AVIN_STATUS_OUT;
				state_changed = true;
				tvafe_pr_info("avin[1].status OUT.\n");
				/*port opened but plug out,need disable clamp*/
				if ((avport_opened & 0x3) == TVAFE_PORT_AV2) {
					W_APB_BIT(TVFE_CLAMP_INTF, 0, CLAMP_EN_BIT, CLAMP_EN_WID);
					tvafe_avin_detect_ch2_dc_enable(true);
				}
			}
			avin_detect_param->s_irq_out_counter1_time = 0;
			avin_detect_param->s_irq_in_counter1_time = 0;
		}
	}
	s_irq_counter1 = av_dev->irq_counter[1];
	return state_changed;
}

static void tvafe_avin_detect_timer_handler(struct timer_list *avin_detect_timer)
{
	bool state_changed = false;
	struct avin_detect_param_s *avin_detect_param;

	if (!av_dev || !meson_data) {
		tvafe_pr_info("tvin av_dev or meson_data is NULL\n");
		return;
	}

	if (avport_opened == 0x80)
		return;

	if ((avin_detect_flag & AVIN_DETECT_OPEN) == 0)
		goto TIMER;

	avin_detect_param = &av_dev->avin_detect_param;
	if (av_dev->dts_param.device_mask & TVAFE_AVIN_CH1_MASK) {
		if (meson_data->detect_version == DETECTED_GPIO_VERSION) {
			if (!tvafe_avin_gpio_detect())
				goto TIMER;
		} else {
			if (!R_HIU_BIT(meson_data->detect_cntl,
				AFE_CH1_EN_DETECT_BIT, AFE_CH1_EN_DETECT_WIDTH))
				goto TIMER;
		}
		av_dev->irq_counter[0] = avin_get_irq_counter(meson_data->irq0_cnt);
	}
	if (av_dev->dts_param.device_mask & TVAFE_AVIN_CH2_MASK) {
		if (!R_HIU_BIT(meson_data->detect_cntl,
			AFE_TL_CH2_EN_DETECT_BIT, AFE_TL_CH2_EN_DETECT_WIDTH))
			goto TIMER;
		av_dev->irq_counter[0] = avin_get_irq_counter(meson_data->irq1_cnt);
	}

	tvafe_avin_get_plug_sts(avin_detect_param);

	if (av_dev->dts_param.device_mask & TVAFE_AVIN_CH1_MASK)
		state_changed = tvafe_avin_detect_ch1(avin_detect_param);
	if (av_dev->dts_param.device_mask & TVAFE_AVIN_CH2_MASK)
		state_changed = tvafe_avin_detect_ch2(avin_detect_param);

	if (avin_detect_debug_print & AVIN_DETECT_STATUS_DBG)
		tvafe_pr_info("av0(%#X:%#x %#x %#x %#x)\n",
			s_irq_counter0, av_dev->irq_counter[0],
			avin_detect_param->s_irq_out_counter0_time,
			avin_detect_param->s_irq_in_counter0_time,
			av_dev->report_data_s[0].status);

	if (state_changed)
		wake_up_interruptible(&tvafe_avin_waitq);

TIMER:
	mod_timer(avin_detect_timer, jiffies +
		(TVAFE_AVIN_INTERVAL * avin_timer_time));
}

int tvafe_avin_init_resource(void)
{
	/* add timer for avin detect*/
	avin_detect_timer.function = tvafe_avin_detect_timer_handler;
	mod_timer(&avin_detect_timer, jiffies +
		(TVAFE_AVIN_INTERVAL * avin_timer_time));

	return 0;
}

static DEVICE_ATTR_RW(debug);

static int tvafe_avin_detect_probe(struct platform_device *pdev)
{
	int ret;
	int state = 0;
	int size_io_reg;
	/*const void *name;*/
	/*int offset, size, mem_size_m;*/
	struct resource res;

	meson_data = (struct meson_avin_data *)
		of_device_get_match_data(&pdev->dev);
	if (meson_data)
		tvafe_pr_info("%s: cpuid:%d,%s.\n",
			      __func__, meson_data->cpu_id, meson_data->name);
	else
		goto get_param_mem_fail;

	av_dev = kzalloc(sizeof(*av_dev), GFP_KERNEL);
	if (!av_dev) {
		state = -ENOMEM;
		goto get_param_mem_fail;
	}

	detect_state = kzalloc(sizeof(*detect_state), GFP_KERNEL);
	if (!detect_state) {
		state = -ENOMEM;
		goto get_param_mem_fail;
	}
	platform_set_drvdata(pdev, av_dev);

	ret = tvafe_avin_dts_parse(pdev);
	if (ret) {
		state = ret;
		goto get_dts_dat_fail;
	}
	detect_state->ch1_state = TVAFE_AVIN_STATUS_UNKNOWN;
	detect_state->ch2_state = TVAFE_AVIN_STATUS_UNKNOWN;
	detect_state->black_cnt = 0;
	/* register char device */
	ret = tvafe_register_avin_dev(av_dev);
	/* create class attr file */
	ret = device_create_file(av_dev->config_dev, &dev_attr_debug);
	if (ret < 0) {
		tvafe_pr_err("fail to create dbg attribute file\n");
		goto fail_create_dbg_file;
	}
	dev_set_drvdata(av_dev->config_dev, av_dev);

	ret = of_address_to_resource(pdev->dev.of_node, 0, &res);
	if (!ret) {
		size_io_reg = resource_size(&res);
		tvafe_pr_info("%s: hiu reg base=0x%p,size=0x%x\n",
			__func__, (void *)res.start, size_io_reg);
		meson_data->irq_reg_base =
			devm_ioremap(&pdev->dev, res.start, size_io_reg);
		if (!meson_data->irq_reg_base) {
			dev_err(&pdev->dev, "hiu ioremap failed\n");
			return -ENOMEM;
		}
		tvafe_pr_info("%s: hiu maped reg_base =0x%p, size=0x%x\n",
			__func__, meson_data->irq_reg_base, size_io_reg);
	} else {
		dev_err(&pdev->dev, "missing hiu memory resource\n");
		meson_data->irq_reg_base = NULL;
	}
	avin_read_reg_wq = create_singlethread_workqueue("t3x_avin_detect");
	INIT_WORK(&avin_read_reg_work, tvafe_avin_detect_ch4);
	/*config analog part setting*/
	tvafe_avin_detect_anlog_config();
	/*config digital part setting*/
	tvafe_avin_detect_digital_config();
	// init count times param
	if (meson_data->detect_version == DETECTED_IRQ_VERSION) {
		av_dev->avin_detect_param.avin_in_count_times = 10;
		av_dev->avin_detect_param.avin_out_count_times = 10;
	} else if (meson_data->detect_version == DETECTED_GPIO_VERSION) {
		av_dev->avin_detect_param.avin_in_count_times = 10;
		av_dev->avin_detect_param.avin_out_count_times = 40;
		//10ms
		avin_timer_time = 1;
	}

	/* add timer for avin detect*/
	timer_setup(&avin_detect_timer, tvafe_avin_detect_timer_handler, 0);
	tvafe_avin_init_resource();

	avin_detect_flag |= AVIN_DETECT_INIT;
	tvafe_pr_info("%s: ok.\n", __func__);

	return 0;

fail_create_dbg_file:
get_dts_dat_fail:
	kfree(av_dev);
get_param_mem_fail:
	tvafe_pr_info("%s: kzalloc error\n", __func__);
	return state;
}

static int tvafe_avin_detect_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	struct tvafe_avin_det_s *av_dev = platform_get_drvdata(pdev);

	del_timer_sync(&avin_detect_timer);
	tvafe_avin_set_detect_en(av_dev, false);
	tvafe_pr_info("%s: tvafe suspend.\n", __func__);
	return 0;
}

static int tvafe_avin_detect_resume(struct platform_device *pdev)
{
	struct tvafe_avin_det_s *av_dev = platform_get_drvdata(pdev);

	tvafe_avin_init_resource();
	tvafe_avin_set_detect_en(av_dev, true);
	tvafe_pr_info("%s: avin resume.\n", __func__);
	return 0;
}

static void tvafe_avin_detect_shutdown(struct platform_device *pdev)
{
	struct tvafe_avin_det_s *av_dev = platform_get_drvdata(pdev);

	avin_detect_flag = 0;
	del_timer_sync(&avin_detect_timer);
	tvafe_avin_set_detect_en(av_dev, false);
	tvafe_pr_info("%s: avin shutdown.\n", __func__);
}

static void tvafe_avin_detect_remove(struct platform_device *pdev)
{
	struct tvafe_avin_det_s *av_dev = platform_get_drvdata(pdev);

	avin_detect_flag = 0;
	if (meson_data->irq_reg_base)
		devm_iounmap(&pdev->dev, meson_data->irq_reg_base);
	cdev_del(&av_dev->avin_cdev);
	del_timer_sync(&avin_detect_timer);
	tvafe_avin_set_detect_en(av_dev, false);
	destroy_workqueue(avin_read_reg_wq);
	avin_read_reg_wq = NULL;
	device_remove_file(av_dev->config_dev, &dev_attr_debug);
	kfree(av_dev);
}

#ifdef CONFIG_OF
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
struct meson_avin_data tl1_data = {
	.cpu_id = AVIN_CPU_TYPE_TL1,
	.name = "meson-tl1-avin-detect",

	.detect_version = DETECTED_IRQ_VERSION,
	.detect_cntl = HHI_CVBS_DETECT_CNTL,
	.irq0_cntl = CVBS_IRQ0_CNTL,
	.irq1_cntl = CVBS_IRQ1_CNTL,
	.irq0_cnt  = CVBS_IRQ0_COUNTER,
	.irq1_cnt  = CVBS_IRQ1_COUNTER,
	.dc_level_adj = 4,
	.vdc_level = 0,
	.comp_level_adj = 3,
};
#endif

struct meson_avin_data tm2_data = {
	.cpu_id = AVIN_CPU_TYPE_TM2,
	.name = "meson-tm2-avin-detect",

	.detect_version = DETECTED_IRQ_VERSION,
	.detect_cntl = HHI_CVBS_DETECT_CNTL,
	.irq0_cntl = CVBS_IRQ0_CNTL,
	.irq1_cntl = CVBS_IRQ1_CNTL,
	.irq0_cnt  = CVBS_IRQ0_COUNTER,
	.irq1_cnt  = CVBS_IRQ1_COUNTER,
	.dc_level_adj = 4,
	.vdc_level = 0,
	.comp_level_adj = 3,
};

struct meson_avin_data t5_data = {
	.cpu_id = AVIN_CPU_TYPE_T5,
	.name = "meson-t5-avin-detect",

	.detect_version = DETECTED_IRQ_VERSION,
	.detect_cntl = HHI_CVBS_DETECT_CNTL,
	.irq0_cntl = CVBS_IRQ0_CNTL,
	.irq1_cntl = CVBS_IRQ1_CNTL,
	.irq0_cnt  = CVBS_IRQ0_COUNTER,
	.irq1_cnt  = CVBS_IRQ1_COUNTER,
	.dc_level_adj = 4,
	.vdc_level = 0,
	.comp_level_adj = 3,
};

struct meson_avin_data t5d_data = {
	.cpu_id = AVIN_CPU_TYPE_T5D,
	.name = "meson-t5d-avin-detect",

	.detect_version = DETECTED_IRQ_VERSION,
	.detect_cntl = HHI_CVBS_DETECT_CNTL,
	.irq0_cntl = CVBS_IRQ0_CNTL,
	.irq1_cntl = CVBS_IRQ1_CNTL,
	.irq0_cnt  = CVBS_IRQ0_COUNTER,
	.irq1_cnt  = CVBS_IRQ1_COUNTER,
	.dc_level_adj = 4,
	.vdc_level = 0,
	.comp_level_adj = 3,
};

struct meson_avin_data t3_data = {
	.cpu_id = AVIN_CPU_TYPE_T3,
	.name = "meson-t3-avin-detect",

	.detect_version = DETECTED_IRQ_VERSION,
	.detect_cntl = ANACTRL_CVBS_DETECT_CNTL,
	.irq0_cntl = IRQCTRL_CVBS_IRQ0_CNTL,
	.irq1_cntl = IRQCTRL_CVBS_IRQ1_CNTL,
	.irq0_cnt  = IRQCTRL_CVBS_IRQ0_COUNTER,
	.irq1_cnt  = IRQCTRL_CVBS_IRQ1_COUNTER,
	.dc_level_adj = 4,
	.vdc_level = 0,
	.comp_level_adj = 3,
};

struct meson_avin_data t5w_data = {
	.cpu_id = AVIN_CPU_TYPE_T5W,
	.name = "meson-t5w-avin-detect",

	.detect_version = DETECTED_IRQ_VERSION,
	.detect_cntl = HHI_CVBS_DETECT_CNTL,
	.irq0_cntl = CVBS_IRQ0_CNTL,
	.irq1_cntl = CVBS_IRQ1_CNTL,
	.irq0_cnt  = CVBS_IRQ0_COUNTER,
	.irq1_cnt  = CVBS_IRQ1_COUNTER,
	.dc_level_adj = 4,
	.vdc_level = 0,
	.comp_level_adj = 3,
};

struct meson_avin_data t5m_data = {
	.cpu_id = AVIN_CPU_TYPE_T5M,
	.name = "meson-t5m-avin-detect",

	.detect_version = DETECTED_IRQ_VERSION,
	.detect_cntl = ANACTRL_CVBS_DETECT_CNTL,
	.irq0_cntl = IRQCTRL_CVBS_IRQ0_CNTL,
	.irq1_cntl = IRQCTRL_CVBS_IRQ1_CNTL,
	.irq0_cnt  = IRQCTRL_CVBS_IRQ0_COUNTER,
	.irq1_cnt  = IRQCTRL_CVBS_IRQ1_COUNTER,
	.dc_level_adj = 4,
	.vdc_level = 0,
	.comp_level_adj = 3,
};

struct meson_avin_data t3x_data = {
	.cpu_id = AVIN_CPU_TYPE_T3X,
	.name = "meson-t3x-avin-detect",

	.detect_version = DETECTED_GPIO_VERSION,
	.detect_cntl = ANACTRL_CVBS_DETECT_CNTL,
	.irq0_cntl = PADCTRL_ANALOG_EN,
	.irq1_cntl = PADCTRL_ANALOG_EN,
	.irq0_cnt  = PADCTRL_ANALOG_I,
	.irq1_cnt  = PADCTRL_ANALOG_I,
	.dc_level_adj = 0,
	.vdc_level = 0,
	.comp_level_adj = 3,
};

struct meson_avin_data txhd2_data = {
	.cpu_id = AVIN_CPU_TYPE_TXHD2,
	.name = "meson-txhd2-avin-detect",

	.detect_version = DETECTED_IRQ_VERSION,
	.detect_cntl = HHI_CVBS_DETECT_CNTL,
	.irq0_cntl = CVBS_IRQ0_CNTL,
	.irq1_cntl = CVBS_IRQ1_CNTL,
	.irq0_cnt  = CVBS_IRQ0_COUNTER,
	.irq1_cnt  = CVBS_IRQ1_COUNTER,
	.dc_level_adj = 3,
	.vdc_level = 3,
	.comp_level_adj = 2,
	.irq_filter = 1,
};

struct meson_avin_data t6d_data = {
	.cpu_id = AVIN_CPU_TYPE_T6D,
	.name = "meson-t6d-avin-detect",

	.detect_version = DETECTED_IRQ_VERSION,
	.detect_cntl = ANACTRL_CVBS_DETECT_CNTL,
	.irq0_cntl = IRQCTRL_CVBS_IRQ0_CNTL,
	.irq1_cntl = IRQCTRL_CVBS_IRQ1_CNTL,
	.irq0_cnt  = IRQCTRL_CVBS_IRQ0_COUNTER,
	.irq1_cnt  = IRQCTRL_CVBS_IRQ1_COUNTER,
	.dc_level_adj = 3,
	.vdc_level = 3,
	.comp_level_adj = 2,
	.irq_filter = 1,
};

struct meson_avin_data t6w_data = {
	.cpu_id = AVIN_CPU_TYPE_T6W,
	.name = "meson-t6w-avin-detect",

	.detect_version = DETECTED_IRQ_VERSION,
	.detect_cntl = ANACTRL_CVBS_DETECT_CNTL_T6W,
	.irq0_cntl = IRQCTRL_CVBS_IRQ0_CNTL,
	.irq1_cntl = IRQCTRL_CVBS_IRQ1_CNTL,
	.irq0_cnt  = IRQCTRL_CVBS_IRQ0_COUNTER,
	.irq1_cnt  = IRQCTRL_CVBS_IRQ1_COUNTER,
	.dc_level_adj = 3,
	.vdc_level = 3,
	.comp_level_adj = 2,
	.irq_filter = 1,
};

struct meson_avin_data t6x_data = {
	.cpu_id = AVIN_CPU_TYPE_T6X,
	.name = "meson-t6x-avin-detect",

	.detect_version = DETECTED_IRQ_VERSION,
	.detect_cntl = ANACTRL_CVBS_DETECT_CNTL + T6X_OFFSET,
	.irq0_cntl = IRQCTRL_CVBS_IRQ0_CNTL,
	.irq1_cntl = IRQCTRL_CVBS_IRQ1_CNTL,
	.irq0_cnt  = IRQCTRL_CVBS_IRQ0_COUNTER,
	.irq1_cnt  = IRQCTRL_CVBS_IRQ1_COUNTER,
	.dc_level_adj = 4,
	.vdc_level = 3,
	.comp_level_adj = 2,
	.irq_filter = 1,
};

static const struct of_device_id tvafe_avin_dt_match[] = {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{	.compatible = "amlogic, tvafe_avin_detect",
	},
	{	.compatible = "amlogic, tl1_tvafe_avin_detect",
		.data = &tl1_data,
	},
#endif
	{	.compatible = "amlogic, tm2_tvafe_avin_detect",
		.data = &tm2_data,
	},
	{	.compatible = "amlogic, t5_tvafe_avin_detect",
		.data = &t5_data,
	},
	{	.compatible = "amlogic, t5d_tvafe_avin_detect",
		.data = &t5d_data,
	},
	{	.compatible = "amlogic, t3_tvafe_avin_detect",
		.data = &t3_data,
	},
	{	.compatible = "amlogic, t5w_tvafe_avin_detect",
		.data = &t5w_data,
	},
	{	.compatible = "amlogic, t5m_tvafe_avin_detect",
		.data = &t5m_data,
	},
	{	.compatible = "amlogic, t3x_tvafe_avin_detect",
		.data = &t3x_data,
	},
	{	.compatible = "amlogic, txhd2_tvafe_avin_detect",
		.data = &txhd2_data,
	},
	{	.compatible = "amlogic, t6d_tvafe_avin_detect",
		.data = &t6d_data,
	},
	{	.compatible = "amlogic, t6w_tvafe_avin_detect",
		.data = &t6w_data,
	},
	{	.compatible = "amlogic, t6x_tvafe_avin_detect",
		.data = &t6x_data,
	},
	{},
};
#else
#define tvafe_avin_dt_match NULL
#endif

static struct platform_driver tvafe_avin_driver = {
	.probe      = tvafe_avin_detect_probe,
	.remove     = tvafe_avin_detect_remove,
	.suspend    = tvafe_avin_detect_suspend,
	.resume     = tvafe_avin_detect_resume,
	.shutdown   = tvafe_avin_detect_shutdown,
	.driver     = {
		.name   = "tvafe_avin_detect",
		.of_match_table = tvafe_avin_dt_match,
	},
};

int __init tvafe_avin_detect_init(void)
{
	return platform_driver_register(&tvafe_avin_driver);
}

void __exit tvafe_avin_detect_exit(void)
{
	cancel_work_sync(&avin_read_reg_work);
	destroy_workqueue(avin_read_reg_wq);
	platform_driver_unregister(&tvafe_avin_driver);
}

//MODULE_DESCRIPTION("Meson TVAFE AVIN detect Driver");
//MODULE_LICENSE("GPL");
//MODULE_AUTHOR("Amlogic, Inc.");

