// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

/*****************************************************************
 **  author :
 **	     Shijie.Rong@amlogic.com
 **  version :
 **	v1.0	  12/3/13
 **	v2.0	 15/10/12
 **	v3.0	  17/11/15
 *****************************************************************/
#define __DVB_CORE__	/*ary 2018-1-31*/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/firmware.h>
#include <linux/err.h>	/*IS_ERR*/
#include <linux/clk.h>	/*clk tree*/
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/crc32.h>

#ifdef ARC_700
#include <asm/arch/am_regs.h>
#else
/* #include <mach/am_regs.h> */
#endif
#include <linux/i2c.h>
#include <linux/gpio.h>

#include "aml_demod.h"
#include "demod_func.h"
#include "demod_dbg.h"
#ifdef AML_DEMOD_SUPPORT_DVBT
#include "dvbt_func.h"
#include "dvbt_frontend.h"
#endif
#ifdef AML_DEMOD_SUPPORT_ISDBT
#include "isdbt_func.h"
#include "isdbt_frontend.h"
#endif
#ifdef AML_DEMOD_SUPPORT_DVBS
#include "dvbs.h"
#include "dvbs_frontend.h"
#include "dvbs_diseqc.h"
#include "dvbs_singlecable.h"
#endif
#ifdef AML_DEMOD_SUPPORT_DVBC
#include "dvbc_func.h"
#include "dvbc_frontend.h"
#endif
#ifdef AML_DEMOD_SUPPORT_DTMB
#include "dtmb_frontend.h"
#endif
#ifdef AML_DEMOD_SUPPORT_ATSC
#include "atsc_frontend.h"
#endif

#ifdef AML_DEMOD_SUPPORT_J83B
#include "j83b_frontend.h"
#endif

/*dma_get_cma_size_int_byte*/
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/dma-map-ops.h>

#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include <linux/amlogic/media/vout/vdac_dev.h>
#include <linux/amlogic/aml_dtvdemod.h>

MODULE_PARM_DESC(dvbc_new_driver, "\n\t\t use dvbc new driver to work");
static unsigned char dvbc_new_driver;
module_param(dvbc_new_driver, byte, 0644);

int aml_demod_debug = DBG_INFO;
module_param(aml_demod_debug, int, 0644);
MODULE_PARM_DESC(aml_demod_debug, "set debug level (info=bit1, reg=bit2, atsc=bit4,");

/*-----------------------------------*/
static struct amldtvdemod_device_s *dtvdd_devp;

static DEFINE_MUTEX(amldtvdemod_device_mutex);

#ifdef AML_DEMOD_SUPPORT_DVBC
static int cci_thread;
#endif

static int dvb_tuner_delay = 100;
module_param(dvb_tuner_delay, int, 0644);
MODULE_PARM_DESC(dvb_atsc_count, "dvb_tuner_delay");

const char *name_reg[] = {
	"demod",
	"iohiu",
	"aobus",
	"reset",
};

#define END_SYS_DELIVERY	19
const char *name_fe_delivery_system[] = {
	"UNDEFINED",
	"DVBC_ANNEX_A",
	"DVBC_ANNEX_B",
	"DVBT",
	"DSS",
	"DVBS",
	"DVBS2",
	"DVBH",
	"ISDBT",
	"ISDBS",
	"ISDBC",
	"ATSC",
	"ATSCMH",
	"DTMB",
	"CMMB",
	"DAB",
	"DVBT2",
	"TURBO",
	"DVBC_ANNEX_C",
	"ANALOG",	/*19*/
};

const char *dtvdemod_get_cur_delsys(enum fe_delivery_system delsys)
{
	if ((delsys >= SYS_UNDEFINED) && (delsys <= SYS_ANALOG))
		return name_fe_delivery_system[delsys];
	else
		return "invalid delsys";
}

static int dtvdemod_leave_mode(struct amldtvdemod_device_s *devp);

struct amldtvdemod_device_s *dtvdemod_get_dev(void)
{
	return dtvdd_devp;
}

int convert_snr(int in_snr)
{
	int out_snr;
	static int calce_snr[40] = {
		5, 6, 8, 10, 13,
		16, 20, 25, 32, 40,
		50, 63, 80, 100, 126,
		159, 200, 252, 318, 400,
		504, 634, 798, 1005, 1265,
		1592, 2005, 2524, 3177, 4000,
		5036, 6340, 7981, 10048, 12649,
		15924, 20047, 25238, 31773, 40000};
	for (out_snr = 1 ; out_snr < 40; out_snr++)
		if (in_snr <= calce_snr[out_snr])
			break;

	return out_snr;
}

//static void dtvdemod_do_8vsb_rst(void)
//{
	//union atsc_cntl_reg_0x20 val;

	//val.bits = atsc_read_reg_v4(ATSC_CNTR_REG_0X20);
	//val.b.cpu_rst = 1;
	//atsc_write_reg_v4(ATSC_CNTR_REG_0X20, val.bits);
	//val.b.cpu_rst = 0;
	//atsc_write_reg_v4(ATSC_CNTR_REG_0X20, val.bits);
//}

//static int amdemod_check_8vsb_rst(struct aml_dtvdemod *demod)
//{
	//int ret = 0;

	/* for tl1/tm2, first time of pwr on,
	 * reset after signal locked
	 * for lock event, 0x79 is safe for reset
	 */
	//if (demod->atsc_rst_done == 0) {
		//if (demod->atsc_rst_needed) {
			//dtvdemod_do_8vsb_rst();
			//demod->atsc_rst_done = 1;
			//ret = 0;
			//PR_ATSC("reset done\n");
		//}

		//if (atsc_read_reg_v4(ATSC_CNTR_REG_0X2E) >= 0x79) {
			//demod->atsc_rst_needed = 1;
			//ret = 0;
			//PR_ATSC("need reset\n");
		//} else {
			//demod->atsc_rst_wait_cnt++;
			//PR_ATSC("wait cnt: %d\n",
					//demod->atsc_rst_wait_cnt);
		//}

		//if (demod->atsc_rst_wait_cnt >= 3 &&
		    //(atsc_read_reg_v4(ATSC_CNTR_REG_0X2E) >= 0x76)) {
			//demod->atsc_rst_done = 1;
			//ret = 1;
		//}
	//} else if (atsc_read_reg_v4(ATSC_CNTR_REG_0X2E) >= 0x76) {
		//ret = 1;
	//}

	//return ret;
//}

unsigned int demod_is_t5d_cpu(struct amldtvdemod_device_s *devp)
{
	enum dtv_demod_hw_ver_e hw_ver = devp->data->hw_ver;

	return (hw_ver == DTVDEMOD_HW_T5D) || (hw_ver == DTVDEMOD_HW_T5D_B);
}

unsigned int demod_get_adc_clk(struct aml_dtvdemod *demod)
{
	return demod->demod_status.adc_freq;
}

unsigned int demod_get_sys_clk(struct aml_dtvdemod *demod)
{
	return demod->demod_status.clk_freq;
}

static enum dvbfe_algo gxtv_demod_get_frontend_algo(struct dvb_frontend *fe)
{
	return DVBFE_ALGO_HW;
}

bool dtvdemod_cma_alloc(struct amldtvdemod_device_s *devp,
		enum fe_delivery_system delsys)
{
	bool ret = true;
#ifdef CONFIG_CMA
	unsigned int mem_size = devp->cma_mem_size;
	int flags = CODEC_MM_FLAGS_CMA_FIRST | CODEC_MM_FLAGS_CMA_CLEAR |
			CODEC_MM_FLAGS_DMA;

	if (!mem_size) {
		PR_INFO("%s: mem_size == 0.\n", __func__);
		return false;
	}

	if (devp->cma_flag) {
		/*	dma_alloc_from_contiguous*/
		devp->venc_pages = dma_alloc_from_contiguous(&devp->this_pdev->dev,
				mem_size >> PAGE_SHIFT, 0, 0);
		if (devp->venc_pages) {
			devp->mem_start = page_to_phys(devp->venc_pages);
			devp->mem_size = mem_size;
			devp->flg_cma_allc = true;
			PR_INFO("%s: cma mem_start = 0x%x, mem_size = 0x%x.\n",
					__func__, devp->mem_start, devp->mem_size);
		} else {
			PR_INFO("%s: cma alloc fail.\n", __func__);
			ret = false;
		}
	} else {
		if (delsys == SYS_ISDBT || delsys == SYS_DTMB) {
			if (mem_size > 16 * SZ_1M)
				mem_size = 16 * SZ_1M;
		} else {
			if (delsys != SYS_DVBT2 && mem_size > 8 * SZ_1M)
				mem_size = 8 * SZ_1M;
		}

		devp->mem_start = codec_mm_alloc_for_dma(DEMOD_DEVICE_NAME,
			mem_size / PAGE_SIZE, 0, flags);
		devp->mem_size = mem_size;
		if (devp->mem_start == 0) {
			PR_INFO("%s: codec_mm fail.\n", __func__);
			ret = false;
		} else {
			devp->flg_cma_allc = true;
			PR_INFO("%s: codec_mm mem_start = 0x%x, mem_size = 0x%x.\n",
					__func__, devp->mem_start, devp->mem_size);
		}
	}
#endif

	return ret;
}

void dtvdemod_cma_release(struct amldtvdemod_device_s *devp)
{
	int ret = 0;

#ifdef CONFIG_CMA

	if (devp->cma_flag)
		ret = dma_release_from_contiguous(&devp->this_pdev->dev,
				devp->venc_pages,
				devp->cma_mem_size >> PAGE_SHIFT);
	else
		ret = codec_mm_free_for_dma(DEMOD_DEVICE_NAME, devp->mem_start);

	devp->mem_start = 0;
	devp->mem_size = 0;
#endif

	PR_DBG("demod cma release: ret %d.\n", ret);
}

static void set_agc_pinmux(struct aml_dtvdemod *demod,
		enum fe_delivery_system delsys, unsigned int on)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();
	struct pinctrl *pin = NULL;
	char *agc_name = NULL;
	char *diseqc_out_name = NULL, *diseqc_in_name = NULL;

	switch (delsys) {
#ifdef AML_DEMOD_SUPPORT_DVBS
	case SYS_DVBS:
	case SYS_DVBS2:
		/* dvbs connects to rf agc pin due to no IF */
		agc_name = "rf_agc_pins";
		diseqc_out_name = "diseqc_out";
		diseqc_in_name = "diseqc_in";
		break;
#endif
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_B:
	case SYS_DVBC_ANNEX_C:
		if (demod->id == 1)
			agc_name = "if_agc2_pins";
		else
			agc_name = "if_agc_pins";
		break;
	default:
		agc_name = "if_agc_pins";
		break;
	}

	if (on) {
		if (agc_name && IS_ERR_OR_NULL(demod->pin_agc)) {
			pin = devm_pinctrl_get_select(devp->dev, agc_name);
			if (IS_ERR_OR_NULL(pin))
				PR_ERR("get pins fail: %s\n", agc_name);
			else
				demod->pin_agc = pin;
		}

		if (diseqc_out_name && IS_ERR_OR_NULL(demod->pin_diseqc_out)) {
			pin = devm_pinctrl_get_select(devp->dev, diseqc_out_name);
			if (IS_ERR_OR_NULL(pin))
				PR_ERR("get pins fail: %s\n", diseqc_out_name);
			else
				demod->pin_diseqc_out = pin;
		}

		if (diseqc_in_name && IS_ERR_OR_NULL(demod->pin_diseqc_in)) {
			pin = devm_pinctrl_get_select(devp->dev, diseqc_in_name);
			if (IS_ERR_OR_NULL(pin))
				PR_ERR("get pins fail: %s\n", diseqc_in_name);
			else
				demod->pin_diseqc_in = pin;
		}
	} else {
		if (!IS_ERR_OR_NULL(demod->pin_agc)) {
			devm_pinctrl_put(demod->pin_agc);
			demod->pin_agc = NULL;
		}

		if (!IS_ERR_OR_NULL(diseqc_out_name)) {
			if (!IS_ERR_OR_NULL(demod->pin_diseqc_out)) {
				devm_pinctrl_put(demod->pin_diseqc_out);
				demod->pin_diseqc_out = NULL;
			}
		}

		if (!IS_ERR_OR_NULL(diseqc_in_name)) {
			if (!IS_ERR_OR_NULL(demod->pin_diseqc_in)) {
				devm_pinctrl_put(demod->pin_diseqc_in);
				demod->pin_diseqc_in = NULL;
			}
		}
	}

	PR_INFO("%s '%s' %d done.\n", __func__, agc_name, on);
}

static void vdac_clk_gate_ctrl(int status)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (status) {
		if (devp->clk_gate_state) {
			PR_INFO("clk_gate is already on\n");
			return;
		}

		if (IS_ERR_OR_NULL(devp->vdac_clk_gate))
			PR_INFO("%s: no vdac_clk_gate\n", __func__);
		else
			clk_prepare_enable(devp->vdac_clk_gate);

		devp->clk_gate_state = 1;
	} else {
		if (devp->clk_gate_state == 0) {
			PR_INFO("clk_gate is already off\n");
			return;
		}

		if (IS_ERR_OR_NULL(devp->vdac_clk_gate))
			PR_INFO("%s: no vdac_clk_gate\n", __func__);
		else
			clk_disable_unprepare(devp->vdac_clk_gate);

		devp->clk_gate_state = 0;
	}
}

/*
 * use dtvdemod_vdac_enable replace vdac_enable
 */
static void dtvdemod_vdac_enable(bool on)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (on) {
		vdac_clk_gate_ctrl(1);
		if (!devp->vdac_enable) {
			vdac_enable(1, VDAC_MODULE_DTV_DEMOD);
			devp->vdac_enable = true;
		}
	} else {
		vdac_clk_gate_ctrl(0);
		if (devp->vdac_enable) {
			vdac_enable(0, VDAC_MODULE_DTV_DEMOD);
			devp->vdac_enable = false;
		}
	}
}

static void demod_32k_ctrl(unsigned int onoff)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!unlikely(devp)) {
		PR_ERR("%s, devp is NULL\n", __func__);
		return;
	}

	if (devp->data->hw_ver != DTVDEMOD_HW_T3 &&
		devp->data->hw_ver != DTVDEMOD_HW_T5M &&
		devp->data->hw_ver != DTVDEMOD_HW_T5W &&
		devp->data->hw_ver != DTVDEMOD_HW_T3X)
		return;

	if (onoff) {
		if (devp->clk_demod_32k_state) {
			PR_INFO("demod_32k is already on\n");
			return;
		}

		if (IS_ERR_OR_NULL(devp->demod_32k)) {
			PR_INFO("%s: no clk demod_32k\n", __func__);
		} else {
			clk_set_rate(devp->demod_32k, 32768);
			clk_prepare_enable(devp->demod_32k);
		}

		devp->clk_demod_32k_state = 1;
	} else {
		if (devp->clk_demod_32k_state == 0) {
			PR_INFO("demod_32k is already off\n");
			return;
		}

		if (IS_ERR_OR_NULL(devp->demod_32k))
			PR_INFO("%s: no clk demod_32k\n", __func__);
		else
			clk_disable_unprepare(devp->demod_32k);

		devp->clk_demod_32k_state = 0;
	}
}

static bool enter_mode(struct aml_dtvdemod *demod, enum fe_delivery_system delsys)
{
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	int ret = 0;

	if (delsys < SYS_ANALOG)
		PR_INFO("%s [id %d]:%s\n", __func__, demod->id,
				name_fe_delivery_system[delsys]);
	else
		PR_ERR("%s [id %d]:%d\n", __func__, demod->id, delsys);

	set_agc_pinmux(demod, delsys, 1);

	/*-------------------*/
	/* must enable the adc ref signal for demod, */
	/*vdac_enable(1, VDAC_MODULE_DTV_DEMOD);*/
	dtvdemod_vdac_enable(1);/*on*/
	demod_32k_ctrl(1);
	demod->en_detect = 0;/**/
#ifdef AML_DEMOD_SUPPORT_DTMB
	dtmb_poll_stop(demod);/*polling mode*/
#endif

	demod->auto_flags_trig = 1;

#ifdef AML_DEMOD_SUPPORT_DVBC
	if (cci_thread)
		if (dvbc_get_cci_task(demod) == 1)
			dvbc_create_cci_task(demod);
#endif

	if (!devp->flg_cma_allc && devp->cma_mem_size) {
		if (!dtvdemod_cma_alloc(devp, delsys)) {
			ret = -ENOMEM;
			return ret;
		}
	}

	switch (delsys) {
#ifdef AML_DEMOD_SUPPORT_DTMB
	case SYS_DTMB:
		ret = Gxtv_Demod_Dtmb_Init(demod);
		if (ret)
			break;

		demod->act_dtmb = true;
		dtmb_set_mem_st(devp->mem_start);

		if (!cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			demod_top_write_reg(DEMOD_TOP_REGC, 0x8);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_DVBC
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		ret = gxtv_demod_dvbc_init(demod, ADC_MODE);
		if (ret)
			break;

		/* The maximum time of signal detection is 3s */
		timer_set_max(demod, D_TIMER_DETECT, demod->timeout_dvbc_ms);
		/* reset is 4s */
		timer_set_max(demod, D_TIMER_SET, 4000);
		PR_DVBC("timeout is %dms\n", demod->timeout_dvbc_ms);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_ATSC
	case SYS_ATSC:
	case SYS_ATSCMH:
		ret = dtvdemod_atsc_init(demod);
		if (ret)
			break;

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			timer_set_max(demod, D_TIMER_DETECT, demod->timeout_atsc_ms);

		PR_ATSC("timeout is %dms\n", demod->timeout_atsc_ms);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_J83B
	case SYS_DVBC_ANNEX_B:
		ret = dtvdemod_atsc_j83b_init(demod);
		if (ret)
			break;

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			timer_set_max(demod, D_TIMER_DETECT, demod->timeout_atsc_ms);

		PR_ATSC("timeout is %dms\n", demod->timeout_atsc_ms);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_DVBT
	case SYS_DVBT:
		ret = dvbt_init(demod);
		if (ret)
			break;

		timer_set_max(demod, D_TIMER_DETECT, demod->timeout_dvbt_ms);
		PR_DVBT("timeout is %dms\n", demod->timeout_dvbt_ms);
		break;

	case SYS_DVBT2:
		if (devp->data->hw_ver == DTVDEMOD_HW_T5D) {
			devp->dmc_saved = dtvdemod_dmc_reg_read(0x274);
			PR_INFO("dmc val 0x%x\n", devp->dmc_saved);
			dtvdemod_dmc_reg_write(0x274, 0x18100000);
		}

		ret = dtvdemod_dvbt2_init(demod);
		if (ret) {
			if (devp->data->hw_ver == DTVDEMOD_HW_T5D) {
				PR_INFO("resume dmc val 0x%x\n", devp->dmc_saved);
				dtvdemod_dmc_reg_write(0x274, devp->dmc_saved);
			}
			break;
		}

		timer_set_max(demod, D_TIMER_DETECT, demod->timeout_dvbt_ms);
		PR_DVBT("timeout is %dms\n", demod->timeout_dvbt_ms);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_ISDBT
	case SYS_ISDBT:
		ret = dvbt_isdbt_init(demod);
		if (ret)
			break;

		/*The maximum time of signal detection is 2s */
		timer_set_max(demod, D_TIMER_DETECT, 2000);
		/*reset is 4s*/
		timer_set_max(demod, D_TIMER_SET, 4000);
		PR_DBG("[im]memstart is %x\n", devp->mem_start);
		dvbt_isdbt_wr_reg((0x10 << 2), devp->mem_start);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_DVBS
	case SYS_DVBS:
	case SYS_DVBS2:
		ret = dtvdemod_dvbs2_init(demod);
		if (ret)
			break;

		aml_diseqc_isr_en(&devp->diseqc, true);
		dvbs2_diseqc_init();
		timer_set_max(demod, D_TIMER_DETECT, demod->timeout_dvbs_ms);
		PR_DVBS("timeout is %dms\n", demod->timeout_dvbs_ms);
		break;
#endif
	default:
		ret = -EINVAL;
		break;
	}

	if (!ret)
		demod->inited = true;
	else
		demod->inited = false;

	return ret;
}

static int leave_mode(struct aml_dtvdemod *demod, enum fe_delivery_system delsys)
{
	bool really_leave = true;
	struct aml_dtvdemod *tmp = NULL;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
#ifdef AML_DEMOD_SUPPORT_DVBS
	struct dtv_frontend_properties *c = &demod->frontend.dtv_property_cache;
	struct aml_diseqc *diseqc = &devp->diseqc;
#endif

	if (delsys < SYS_ANALOG)
		PR_INFO("%s [id %d]:%s\n", __func__, demod->id,
				name_fe_delivery_system[delsys]);
	else
		PR_ERR("%s [id %d]:%d\n", __func__, demod->id, delsys);

	demod->en_detect = 0;
	demod->last_delsys = SYS_UNDEFINED;
	demod->last_status = 0;

	list_for_each_entry(tmp, &devp->demod_list, list) {
		if (tmp->id != demod->id && tmp->inited) {
			really_leave = false;
			break;
		}
	}

	if (really_leave)
		dtvpll_init_flag(0);

	/*dvbc_timer_exit();*/
#ifdef AML_DEMOD_SUPPORT_DVBC
	if (cci_thread)
		dvbc_kill_cci_task(demod);
#endif

	switch (delsys) {
#ifdef AML_DEMOD_SUPPORT_DVBC
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		demod->last_qam_mode = QAM_MODE_NUM;
		if (really_leave && devp->dvbc_inited)
			devp->dvbc_inited = false;

		break;
#endif
	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		demod->last_qam_mode = QAM_MODE_NUM;
		demod->atsc_mode = 0;
		break;
#ifdef AML_DEMOD_SUPPORT_DTMB
	case SYS_DTMB:
		if (demod->act_dtmb) {
			dtmb_poll_stop(demod); /*polling mode*/
			/* close arbit */
			demod_top_write_reg(DEMOD_TOP_REG0, 0x0);
			demod_top_write_reg(DEMOD_TOP_REGC, 0x0);
			demod->act_dtmb = false;
		}
		break;
#endif
	case SYS_DVBT:
		/* disable dvbt mode to avoid hang when switch to other demod */
		demod_top_write_reg(DEMOD_TOP_REGC, 0x11);
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);
		break;

	case SYS_ISDBT:
	case SYS_DVBT2:
		if (devp->data->hw_ver == DTVDEMOD_HW_T5D && delsys == SYS_DVBT2) {
			PR_INFO("resume dmc val 0x%x\n", devp->dmc_saved);
			dtvdemod_dmc_reg_write(0x274, devp->dmc_saved);
		}

		/* disable dvbt mode to avoid hang when switch to other demod */
		demod_top_write_reg(DEMOD_TOP_REGC, 0x11);
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);
		break;
#ifdef AML_DEMOD_SUPPORT_DVBS
	case SYS_DVBS:
	case SYS_DVBS2:
		/*disable irq*/
		aml_diseqc_isr_en(&devp->diseqc, false);
		/* disable dvbs mode to avoid hang when switch to other demod */
		demod_top_write_reg(DEMOD_TOP_REGC, 0x11);
		aml_diseqc_set_lnb_voltage(diseqc, SEC_VOLTAGE_OFF);
		c->voltage = SEC_VOLTAGE_OFF;
		break;
#endif
	default:
		break;
	}

	if (really_leave) {
#ifdef CONFIG_AMLOGIC_MEDIA_ADC
		adc_set_pll_cntl(0, ADC_DTV_DEMOD, NULL);
		adc_set_pll_cntl(0, ADC_DTV_DEMODPLL, NULL);
#endif
		/* should disable the adc ref signal for demod */
		/*vdac_enable(0, VDAC_MODULE_DTV_DEMOD);*/
		dtvdemod_vdac_enable(0);/*off*/
		demod_32k_ctrl(0);
		set_agc_pinmux(demod, delsys, 0);

		if (devp->flg_cma_allc && devp->cma_mem_size) {
			dtvdemod_cma_release(devp);
			devp->flg_cma_allc = false;
		}

		PR_INFO("%s: really_leave.\n", __func__);
	}

	demod->inited = false;
	demod->suspended = true;

	return 0;
}

static void delsys_exit(struct aml_dtvdemod *demod, unsigned int ldelsys,
		unsigned int cdelsys)
{
	struct dvb_frontend *fe = &demod->frontend;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	unsigned int abus_en_dly = 0, polling_en = 0, top_saved = 0;
	int retry_count = 2;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (is_meson_t3_cpu() && ldelsys == SYS_DTMB) {
#ifdef AML_DEMOD_SUPPORT_DTMB
		dtmb_write_reg(0x7, 0x6ffffd);
		//dtmb_write_reg(0x47, 0xed33221);
		dtmb_write_reg_bits(0x47, 0x1, 22, 1);
		dtmb_write_reg_bits(0x47, 0x1, 23, 1);

		if (is_meson_t3_cpu() && is_meson_rev_b())
			t3_revb_set_ambus_state(true, false);
#endif
	} else if ((is_meson_t5w_cpu() || is_meson_t3_cpu() ||
		demod_is_t5d_cpu(devp)) && ldelsys == SYS_DVBT2) {
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x182);
		dvbt_t2_wr_byte_bits(0x09, 1, 4, 1);
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x97);
		riscv_ctl_write_reg(0x30, 4);
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x182);
		dvbt_t2_wr_byte_bits(0x07, 1, 7, 1);
		dvbt_t2_wr_byte_bits(0x3613, 0, 4, 3);
		dvbt_t2_wr_byte_bits(0x3617, 0, 0, 3);

		if (is_meson_t3_cpu() && is_meson_rev_b())
			t3_revb_set_ambus_state(true, true);

		if (is_meson_t5w_cpu())
			t5w_write_ambus_reg(0x3c4e, 0x1, 23, 1);
	} else if ((is_meson_t5w_cpu() || is_meson_t3_cpu() ||
		demod_is_t5d_cpu(devp)) && ldelsys == SYS_ISDBT) {
		dvbt_isdbt_wr_reg((0x2 << 2), 0x111021b);
		dvbt_isdbt_wr_reg((0x2 << 2), 0x011021b);
		dvbt_isdbt_wr_reg((0x2 << 2), 0x011001b);

		if (is_meson_t3_cpu() && is_meson_rev_b())
			t3_revb_set_ambus_state(true, false);

		if (is_meson_t5w_cpu())
			t5w_write_ambus_reg(0x3c4e, 0x1, 23, 1);
	}
#endif

	//disable demod output AMBUS LSB signal
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T5M) && (ldelsys == SYS_DTMB ||
			ldelsys == SYS_DVBT2 || ldelsys == SYS_ISDBT)) {
		if (ldelsys == SYS_DVBT2) {
			//set f040 = 0x0, disable t/t2 mode, stop to
			//access t/t2 regs, so should stop demod_thread to access t/t2 status first.
			polling_en = devp->demod_thread;
			devp->demod_thread = 0;
			top_saved = demod_top_read_reg(DEMOD_TOP_CFG_REG_4);
			demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);
		}

		//0x38e4[29], enable abus_en delay logic
		front_write_bits(DEMOD_FRONT_REG39, 0x1, 29, 1);
		//0x38e4[30], set dc_arb_enable = 0
		front_write_bits(DEMOD_FRONT_REG39, 0x0, 30, 1);

		//0x38e0[31], when read abus_en_dly = 0,
		//then continue the following flow of closing demod.
		abus_en_dly = front_read_reg(DEMOD_FRONT_REG38) & 0x80000000;
		PR_INFO("%s: 0x38e0 %#x, abus_en_dly %d\n", __func__,
					front_read_reg(DEMOD_FRONT_REG38), abus_en_dly);
		while (abus_en_dly && retry_count--) {
			msleep(20);
			abus_en_dly = front_read_reg(DEMOD_FRONT_REG38) & 0x80000000;
			PR_INFO("%s: retry_count %d 0x38e0 %#x\n", __func__, retry_count,
					front_read_reg(DEMOD_FRONT_REG38));
		}
		if (abus_en_dly)
			PR_ERR("%s: abus_en_dly ERROR ERROR!\n", __func__);

		if (ldelsys == SYS_DVBT2) {
			//f040 = 0x182: host only can access top regs and t2 regs
			demod_top_write_reg(DEMOD_TOP_CFG_REG_4, top_saved);
			devp->demod_thread = polling_en;
		}
	}

	if (fe->ops.tuner_ops.release &&
		(cdelsys == SYS_UNDEFINED || cdelsys == SYS_ANALOG))
		fe->ops.tuner_ops.release(fe);

	if ((is_meson_t5w_cpu() || is_meson_t3_cpu() ||
		demod_is_t5d_cpu(devp)) &&
		(ldelsys == SYS_DTMB ||
		ldelsys == SYS_DVBT2 ||
		ldelsys == SYS_ISDBT)) {
		msleep(demod->timeout_ddr_leave);
		clear_ddr_bus_data(demod);
	}

	leave_mode(demod, ldelsys);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (is_meson_t5w_cpu() &&
		(ldelsys == SYS_DVBT2 || ldelsys == SYS_ISDBT)) {
		msleep(20);

		t5w_write_ambus_reg(0x3c4e, 0x0, 23, 1);
	} else if (is_meson_t3_cpu() && is_meson_rev_b() &&
			(ldelsys == SYS_DTMB ||
			ldelsys == SYS_DVBT2 ||
			ldelsys == SYS_ISDBT)) {
		msleep(20);

		t3_revb_set_ambus_state(false, ldelsys == SYS_DVBT2);
	}
#endif
}

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
/* when can't get ic_config by dts, use this*/
const struct meson_ddemod_data  data_gxtvbb = {
	.dig_clk = {
		.demod_clk_ctl = 0x74,
		.demod_clk_ctl_1 = 0x75,
	},
	.regoff = {
		.off_demod_top = 0xc00,
		.off_dvbc = 0x400,
		.off_dtmb = 0x00,
	},
};

const struct meson_ddemod_data  data_txlx = {
	.dig_clk = {
		.demod_clk_ctl = 0x74,
		.demod_clk_ctl_1 = 0x75,
	},
	.regoff = {
		.off_demod_top = 0xf00,
		.off_dvbc = 0xc00,
		.off_dtmb = 0x000,
		.off_dvbt_isdbt = 0x400,
		.off_isdbt = 0x400,
		.off_atsc = 0x800,
	},
	.hw_ver = DTVDEMOD_HW_TXLX,
};
#endif

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
const struct meson_ddemod_data  data_tl1 = {
	.dig_clk = {
		.demod_clk_ctl = 0x74,
		.demod_clk_ctl_1 = 0x75,
	},
	.regoff = {
		.off_demod_top = 0x3c00,
		.off_dvbc = 0x1000,
		.off_dtmb = 0x0000,
		.off_atsc = 0x0c00,
		.off_front = 0x3800,
	},
	.hw_ver = DTVDEMOD_HW_TL1,
};

const struct meson_ddemod_data data_tm2 = {
	.dig_clk = {
		.demod_clk_ctl = 0x74,
		.demod_clk_ctl_1 = 0x75,
	},
	.regoff = {
		.off_demod_top = 0x3c00,
		.off_dvbc = 0x1000,
		.off_dtmb = 0x0000,
		.off_atsc = 0x0c00,
		.off_front = 0x3800,
	},
	.hw_ver = DTVDEMOD_HW_TM2,
};

const struct meson_ddemod_data  data_t5 = {
	.dig_clk = {
		.demod_clk_ctl = 0x74,
		.demod_clk_ctl_1 = 0x75,
	},
	.regoff = {
		.off_demod_top = 0x3c00,
		.off_dvbc = 0x1000,
		.off_dtmb = 0x0000,
		.off_front = 0x3800,
	},
	.hw_ver = DTVDEMOD_HW_T5,
};

const struct meson_ddemod_data  data_t5d = {
	.dig_clk = {
		.demod_clk_ctl = 0x74,
		.demod_clk_ctl_1 = 0x75,
	},
	.regoff = {
		.off_demod_top = 0xf000,
		.off_dvbc = 0x1000,
		.off_dtmb = 0x0000,
		.off_atsc = 0x0c00,
		.off_isdbt = 0x800,
		.off_front = 0x3800,
		.off_dvbs = 0x2000,
		.off_dvbt_isdbt = 0x800,
		.off_dvbt_t2 = 0x0000,
	},
	.hw_ver = DTVDEMOD_HW_T5D,
};

const struct meson_ddemod_data  data_s4 = {
	.dig_clk = {
		.demod_clk_ctl = 0x74,
		.demod_clk_ctl_1 = 0x75,
	},
	.regoff = {
		.off_demod_top = 0xf000,
		.off_front = 0x3800,
		.off_dvbc = 0x1000,
		.off_dvbc_2 = 0x1400,
	},
	.hw_ver = DTVDEMOD_HW_S4,
};

const struct meson_ddemod_data  data_t5d_revb = {
	.dig_clk = {
		.demod_clk_ctl = 0x74,
		.demod_clk_ctl_1 = 0x75,
	},
	.regoff = {
		.off_demod_top = 0xf000,
		.off_dvbc = 0x1000,
		.off_dtmb = 0x0000,
		.off_atsc = 0x0c00,
		.off_isdbt = 0x800,
		.off_front = 0x3800,
		.off_dvbs = 0x2000,
		.off_dvbt_isdbt = 0x800,
		.off_dvbt_t2 = 0x0000,
	},
	.hw_ver = DTVDEMOD_HW_T5D_B,
};

const struct meson_ddemod_data  data_t3 = {
	.dig_clk = {
		.demod_clk_ctl = 0x82,
		.demod_clk_ctl_1 = 0x83,
	},
	.regoff = {
		.off_demod_top = 0xf000,
		.off_dvbc = 0x1000,
		.off_dtmb = 0x0000,
		.off_atsc = 0x0c00,
		.off_isdbt = 0x800,
		.off_front = 0x3800,
		.off_dvbs = 0x2000,
		.off_dvbt_isdbt = 0x800,
		.off_dvbt_t2 = 0x0000,
	},
	.hw_ver = DTVDEMOD_HW_T3,
};
#endif

#ifndef CONFIG_AMLOGIC_ZAPPER_C1A
const struct meson_ddemod_data  data_s4d = {
	.dig_clk = {
		.demod_clk_ctl = 0x74,
		.demod_clk_ctl_1 = 0x75,
	},
	.regoff = {
		.off_demod_top = 0xf000,
		.off_front = 0x3800,
		.off_dvbc = 0x1000,
		.off_dvbc_2 = 0x1400,
		.off_dvbs = 0x2000,
	},
	.hw_ver = DTVDEMOD_HW_S4D,
};
#endif

const struct meson_ddemod_data  data_s1a = {
	.dig_clk = {
		.demod_clk_ctl = 0x80,
		.demod_clk_ctl_1 = 0x81,
	},
	.regoff = {
		.off_demod_top = 0xf000,
		.off_front = 0x3800,
		.off_dvbc = 0x1000,
		.off_dvbc_2 = 0x1400,
		.off_dvbs = 0x2000,
	},
	.hw_ver = DTVDEMOD_HW_S1A,
};

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
const struct meson_ddemod_data  data_t5w = {
	.dig_clk = {
		.demod_clk_ctl = 0x74,
		.demod_clk_ctl_1 = 0x75,
	},
	.regoff = {
		.off_demod_top = 0xf000,
		.off_dvbc = 0x1000,
		.off_dtmb = 0x0000,
		.off_atsc = 0x0c00,
		.off_isdbt = 0x800,
		.off_front = 0x3800,
		.off_dvbs = 0x2000,
		.off_dvbt_isdbt = 0x800,
		.off_dvbt_t2 = 0x0000,
	},
	.hw_ver = DTVDEMOD_HW_T5W,
};

const struct meson_ddemod_data  data_t5m = {
	.dig_clk = {
		.demod_clk_ctl = 0x82,
		.demod_clk_ctl_1 = 0x83,
	},
	.regoff = {
		.off_demod_top = 0xf000,
		.off_dvbc = 0x1000,
		.off_dtmb = 0x0000,
		.off_atsc = 0x0c00,
		.off_isdbt = 0x800,
		.off_front = 0x3800,
		.off_dvbs = 0x2000,
		.off_dvbt_isdbt = 0x800,
		.off_dvbt_t2 = 0x0000,
	},
	.hw_ver = DTVDEMOD_HW_T5M,
};

const struct meson_ddemod_data data_t3x = {
	.dig_clk = {
		.demod_clk_ctl = 0x8d,
		.demod_clk_ctl_1 = 0x8e,
	},
	.regoff = {
		.off_demod_top = 0xf000,
		.off_dvbc = 0x1000,
		.off_dtmb = 0x0000,
		.off_atsc = 0x0c00,
		.off_isdbt = 0x800,
		.off_front = 0x3800,
		.off_dvbs = 0x2000,
		.off_dvbt_isdbt = 0x800,
		.off_dvbt_t2 = 0x0000,
	},
	.hw_ver = DTVDEMOD_HW_T3X,
};

const struct meson_ddemod_data data_txhd2 = {
	.dig_clk = {
		.demod_clk_ctl = 0x74,
		.demod_clk_ctl_1 = 0x75,
	},
	.regoff = {
		.off_demod_top = 0xf000,
		.off_dtmb = 0x0000,
		.off_front = 0x3800,
	},
	.hw_ver = DTVDEMOD_HW_TXHD2,
};
#endif

static const struct of_device_id meson_ddemod_match[] = {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible = "amlogic, ddemod-gxtvbb",
		.data		= &data_gxtvbb,
	}, {
		.compatible = "amlogic, ddemod-txl",
		.data		= &data_gxtvbb,
	}, {
		.compatible = "amlogic, ddemod-txlx",
		.data		= &data_txlx,
	}, {
		.compatible = "amlogic, ddemod-gxlx",
		.data		= &data_txlx,
	}, {
		.compatible = "amlogic, ddemod-txhd",
		.data		= &data_txlx,
	}, {
		.compatible = "amlogic, ddemod-tl1",
		.data		= &data_tl1,
	},
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	{
		.compatible = "amlogic, ddemod-tm2",
		.data		= &data_tm2,
	}, {
		.compatible = "amlogic, ddemod-t5",
		.data		= &data_t5,
	}, {
		.compatible = "amlogic, ddemod-t5d",
		.data		= &data_t5d,
	}, {
		.compatible = "amlogic, ddemod-s4",
		.data		= &data_s4,
	}, {
		.compatible = "amlogic, ddemod-t5d-revB",
		.data		= &data_t5d_revb,
	}, {
		.compatible = "amlogic, ddemod-t3",
		.data		= &data_t3,
	},
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_C1A
	{
		.compatible = "amlogic, ddemod-s4d",
		.data		= &data_s4d,
	},
#endif
	{
		.compatible = "amlogic, ddemod-s1a",
		.data		= &data_s1a,
	},
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	{
		.compatible = "amlogic, ddemod-t5w",
		.data		= &data_t5w,
	}, {
		.compatible = "amlogic, ddemod-t5m",
		.data		= &data_t5m,
	}, {
		.compatible = "amlogic, ddemod-t3x",
		.data		= &data_t3x,
	}, {
		.compatible = "amlogic, ddemod-txhd2",
		.data		= &data_txhd2,
	},
#endif
	/* DO NOT remove, to avoid scan err of KASAN */
	{}
};



/*
 * dds_init_reg_map - physical addr map
 *
 * map physical address of I/O memory resources
 * into the core virtual address space
 */
static int dds_init_reg_map(struct platform_device *pdev)
{
	struct amldtvdemod_device_s *devp =
			(struct amldtvdemod_device_s *)platform_get_drvdata(pdev);

	struct ss_reg_phy *preg = &devp->reg_p[0];
	struct ss_reg_vt *pv = &devp->reg_v[0];
	int i;
	struct resource *res = 0;
	int size = 0;
	int ret = 0;

	for (i = 0; i < ES_MAP_ADDR_NUM; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			PR_ERR("%s: res %d is faile\n", __func__, i);
			ret = -ENOMEM;
			break;
		}
		size = resource_size(res);
		preg[i].size = size;
		preg[i].phy_addr = res->start;
		pv[i].v = devm_ioremap(&pdev->dev, res->start, size);
	}

	switch (devp->data->hw_ver) {
	case DTVDEMOD_HW_T5D:
		devp->dmc_phy_addr = 0xff638000;
		devp->dmc_v_addr = devm_ioremap(&pdev->dev, 0xff638000, 0x2000);
		devp->ddr_phy_addr = 0xff63c000;
		devp->ddr_v_addr = devm_ioremap(&pdev->dev, 0xff63c000, 0x2000);
		break;

	case DTVDEMOD_HW_T5D_B:
		devp->ddr_phy_addr = 0xff63c000;
		devp->ddr_v_addr = devm_ioremap(&pdev->dev, 0xff63c000, 0x2000);
		break;

	case DTVDEMOD_HW_T3:
	case DTVDEMOD_HW_T5M:
	case DTVDEMOD_HW_T3X:
		devp->ddr_phy_addr = 0xfe000000;
		devp->ddr_v_addr = devm_ioremap(&pdev->dev, 0xfe000000, 0x2000);
		break;

	case DTVDEMOD_HW_T5W:
	case DTVDEMOD_HW_TXHD2:
		break;

	default:
		break;
	}

	return ret;
}

int dtvdemod_set_iccfg_by_dts(struct platform_device *pdev)
{
	u32 value;
	int ret;
#ifndef CONFIG_OF
	struct resource *res = NULL;
#endif
	struct amldtvdemod_device_s *devp =
			(struct amldtvdemod_device_s *)platform_get_drvdata(pdev);

	PR_DBG("%s:\n", __func__);

	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret != 0)
		PR_INFO("no reserved mem.\n");

	//dvb-s/s2 tuner agc pin direction set
	//have "agc_pin_direction" agc_direction = 1;donot have agc_direction = 0
	devp->agc_direction = of_property_read_bool(pdev->dev.of_node, "agc_pin_direction");
	PR_INFO("agc_pin_direction:%d\n", devp->agc_direction);

#ifdef CONFIG_OF
	ret = of_property_read_u32(pdev->dev.of_node, "atsc_version", &value);
	if (!ret) {
		devp->atsc_version = value;
		PR_INFO("atsc_version: %d\n", value);
	} else {
		devp->atsc_version = 0;
	}
#else
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					"atsc_version");
	if (res) {
		int atsc_version = res->start;

		devp->atsc_version = atsc_version;
	} else {
		devp->atsc_version = 0;
	}
#endif

#ifdef CONFIG_CMA
	/* Get the actual CMA of Demod in dts. */
	/* If NULL, get it from the codec CMA. */
	/* DTMB(8M)/DVB-T2(40M)/ISDB-T(8M) requires CMA, and others do not. */
	if (of_parse_phandle(pdev->dev.of_node, "memory-region", 0)) {
		devp->cma_mem_size =
			dma_get_cma_size_int_byte(&pdev->dev);
		devp->cma_flag = 1;
	} else {
		devp->cma_flag = 0;
#ifdef CONFIG_OF
		ret = of_property_read_u32(pdev->dev.of_node,
				"cma_mem_size", &value);
		if (!ret)
			devp->cma_mem_size = value * SZ_1M;
		else
			devp->cma_mem_size = 0;
#else
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
				"cma_mem_size");
		if (res)
			devp->cma_mem_size = res->start * SZ_1M;
		else
			devp->cma_mem_size = 0;
#endif
	}

	devp->this_pdev = pdev;
	devp->cma_mem_alloc = 0;
	PR_INFO("demod cma_flag %d, cma_mem_size %d MB.\n",
		devp->cma_flag, (u32)devp->cma_mem_size / SZ_1M);
#endif

	/* diseqc sel */
#ifdef AML_DEMOD_SUPPORT_DVBS
	ret = of_property_read_string(pdev->dev.of_node, "diseqc_name",
			&devp->diseqc.name);
	if (ret) {
		PR_INFO("diseqc name: not define in dts.\n");
		devp->diseqc.name = NULL;
	} else {
		PR_INFO("diseqc name: %s.\n", devp->diseqc.name);
	}

	/*get demod irq*/
	ret = of_irq_get_byname(pdev->dev.of_node, "demod_isr");
	if (ret > 0) {
		devp->diseqc.irq_num = ret;
		ret = request_irq(devp->diseqc.irq_num, aml_diseqc_isr_handler,
				IRQF_SHARED, "demod_diseqc_isr", (void *)devp);
		if (ret != 0)
			PR_INFO("request demod_diseqc_isr fail(%d).\n", ret);

		disable_irq(devp->diseqc.irq_num);
		devp->diseqc.irq_en = false;
		PR_INFO("demod_isr num:%d\n", devp->diseqc.irq_num);
	} else {
		devp->diseqc.irq_num = 0;
		PR_INFO("no diseqc isr.\n");
	}

	ret = of_property_read_u32(pdev->dev.of_node, "iq_swap", &value);
	if (!ret) {
		dvbs_set_iq_swap(value);
		PR_INFO("iq_swap: %d.\n", value);
	}
#endif
	return 0;
}

/* It's a correspondence with enum es_map_addr*/

void dbg_ic_cfg(struct amldtvdemod_device_s *devp)
{
	struct ss_reg_phy *preg = &devp->reg_p[0];
	int i;

	for (i = 0; i < ES_MAP_ADDR_NUM; i++)
		PR_INFO("reg:%s:st=0x%x,size=0x%x\n",
			name_reg[i], preg[i].phy_addr, preg[i].size);
}

void dbg_reg_addr(struct amldtvdemod_device_s *devp)
{
	struct ss_reg_vt *regv = &devp->reg_v[0];
	int i;

	PR_INFO("%s\n", __func__);

	PR_INFO("reg address offset:\n");
	PR_INFO("\tdemod top:\t0x%x\n", devp->data->regoff.off_demod_top);
	PR_INFO("\tdvbc:\t0x%x\n", devp->data->regoff.off_dvbc);
	PR_INFO("\tdtmb:\t0x%x\n", devp->data->regoff.off_dtmb);
	PR_INFO("\tdvbt/isdbt:\t0x%x\n", devp->data->regoff.off_dvbt_isdbt);
	PR_INFO("\tisdbt:\t0x%x\n", devp->data->regoff.off_isdbt);
	PR_INFO("\tatsc:\t0x%x\n", devp->data->regoff.off_atsc);
	PR_INFO("\tfront:\t0x%x\n", devp->data->regoff.off_front);
	PR_INFO("\tdvbt/t2:\t0x%x\n", devp->data->regoff.off_dvbt_t2);

	PR_INFO("virtual addr:\n");
	for (i = 0; i < ES_MAP_ADDR_NUM; i++)
		PR_INFO("\t%s:\t0x%p\n", name_reg[i], regv[i].v);
}

static void dtvdemod_clktree_probe(struct device *dev)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	devp->clk_gate_state = 0;
	devp->clk_demod_32k_state = 0;

	devp->vdac_clk_gate = devm_clk_get(dev, "vdac_clk_gate");
	if (!IS_ERR_OR_NULL(devp->vdac_clk_gate))
		PR_INFO("%s: clk vdac_clk_gate probe ok.\n", __func__);

	devp->demod_32k = devm_clk_get(dev, "demod_32k");
	if (!IS_ERR_OR_NULL(devp->demod_32k))
		PR_INFO("%s: clk demod_32k probe ok.\n", __func__);
}

static void dtvdemod_clktree_remove(struct device *dev)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!IS_ERR_OR_NULL(devp->vdac_clk_gate))
		devm_clk_put(dev, devp->vdac_clk_gate);

	if (!IS_ERR_OR_NULL(devp->demod_32k))
		devm_clk_put(dev, devp->demod_32k);
}

static int dtvdemod_request_firmware(const char *file_name, char *buf, int size)
{
	int ret = -1;
	const struct firmware *fw;
	int offset = 0;
	unsigned int i;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!buf) {
		pr_err("%s fw buf is NULL\n", __func__);
		ret = -ENOMEM;
		goto err;
	}

	ret = request_firmware(&fw, file_name, devp->dev);
	if (ret < 0) {
		pr_err("%d can't load the %s.\n", ret, file_name);
		goto err;
	}

	if (fw->size > size) {
		pr_err("not enough memory size for fw.\n");
		ret = -ENOMEM;
		goto release;
	}

	memcpy(buf, (char *)fw->data + offset, fw->size - offset);
	ret = fw->size;

	PR_DBGL("dtvdemod_request_firmware:\n");
	for (i = 0; i < 100; i++)
		PR_DBGL("[%d] = 0x%x\n", i, fw->data[i]);
release:
	release_firmware(fw);
err:
	return ret;
}

static int fw_check_sum(char *buf, unsigned int len)
{
	unsigned int crc;

	crc = crc32_le(~0U, buf, len);

	PR_INFO("firmware crc result : 0x%x, len: %d\n", crc ^ ~0U, len);

	/* return fw->head.checksum != (crc ^ ~0U) ? 0 : 1; */
	return 0;
}

static int dtvdemod_download_firmware(struct amldtvdemod_device_s *devp)
{
	char *path = __getname();
	int len = 0;
	int size = 0;

	if (!path)
		return -ENOMEM;

	len = snprintf(path, PATH_MAX_LEN, "%s/%s", FIRMWARE_DIR, FIRMWARE_NAME);
	if (len >= PATH_MAX_LEN)
		goto path_fail;

	strncpy(devp->firmware_path, path, sizeof(devp->firmware_path));
	devp->firmware_path[sizeof(devp->firmware_path) - 1] = '\0';

	size = dtvdemod_request_firmware(devp->firmware_path, devp->fw_buf, FW_BUFF_SIZE);

	if (size >= 0)
		fw_check_sum(devp->fw_buf, size);

path_fail:
	__putname(path);

	return size;
}

static void dtvdemod_fw_dwork(struct work_struct *work)
{
	int ret;
	static unsigned int cnt;
	struct delayed_work *dwork = to_delayed_work(work);
	struct amldtvdemod_device_s *devp =
		container_of(dwork, struct amldtvdemod_device_s, fw_dwork);

	if (!devp) {
		pr_info("%s, dwork error !!!\n", __func__);
		return;
	}

	ret = dtvdemod_download_firmware(devp);

	if ((ret < 0) && (cnt < 10))
		schedule_delayed_work(&devp->fw_dwork, 5 * HZ);
	else
		cancel_delayed_work(&devp->fw_dwork);

	cnt++;
}

/* platform driver*/
static int aml_dtvdemod_probe(struct platform_device *pdev)
{
	int ret = 0;
	const struct of_device_id *match;
	struct amldtvdemod_device_s *devp;

	PR_INFO("%s\n", __func__);
	/*memory*/
	dtvdd_devp = kzalloc(sizeof(*dtvdd_devp), GFP_KERNEL);
	devp = dtvdd_devp;

	if (!devp)
		goto fail_alloc_region;

	devp->dev = &pdev->dev;
	platform_set_drvdata(pdev, devp);

	/*class attr */
	devp->clsp = class_create(THIS_MODULE, DEMOD_DEVICE_NAME);
	if (!devp->clsp)
		goto fail_create_class;

	ret = dtvdemod_create_class_files(devp->clsp);
	if (ret)
		goto fail_class_create_file;

	match = of_match_device(meson_ddemod_match, &pdev->dev);
	if (match == NULL) {
		PR_ERR("%s,no matched table\n", __func__);
		goto fail_ic_config;
	}
	devp->data = (struct meson_ddemod_data *)match->data;

	/*reg*/
	ret = dds_init_reg_map(pdev);
	if (ret)
		goto fail_ic_config;

	ret = dtvdemod_set_iccfg_by_dts(pdev);
	if (ret)
		goto fail_ic_config;

	/*debug:*/
	dbg_ic_cfg(dtvdd_devp);
	dbg_reg_addr(dtvdd_devp);

	/**/
	dtvpll_lock_init();
	demod_init_mutex();
	INIT_LIST_HEAD(&devp->demod_list);
	mutex_init(&devp->lock);
#ifdef AML_DEMOD_SUPPORT_DVBS
	mutex_init(&devp->diseqc.mutex_tx_msg);
	mutex_init(&devp->diseqc.mutex_rx_msg);
#endif
	dtvdemod_clktree_probe(&pdev->dev);
#ifdef AML_DEMOD_SUPPORT_ATSC
	atsc_set_version(devp->atsc_version);
#endif

	devp->flg_cma_allc = false;
	devp->demod_thread = 1;
	devp->blind_scan_stop = 1;
	devp->blind_debug_max_frc = 0;
	devp->blind_debug_min_frc = 0;
	devp->blind_same_frec = 0;

	//ary temp:
	aml_demod_init();

	if (devp->data->hw_ver >= DTVDEMOD_HW_T5D) {
		pm_runtime_enable(devp->dev);
		if (pm_runtime_get_sync(devp->dev) < 0)
			pr_err("failed to set pwr\n");

		devp->fw_buf = kzalloc(FW_BUFF_SIZE, GFP_KERNEL);
		if (!devp->fw_buf)
			ret = -ENOMEM;

		/* delayed workqueue for dvbt2 fw downloading */
		if (dtvdd_devp->data->hw_ver != DTVDEMOD_HW_S4 &&
			dtvdd_devp->data->hw_ver != DTVDEMOD_HW_S4D &&
			dtvdd_devp->data->hw_ver != DTVDEMOD_HW_TXHD2 &&
			dtvdd_devp->data->hw_ver != DTVDEMOD_HW_S1A) {
			INIT_DELAYED_WORK(&devp->fw_dwork, dtvdemod_fw_dwork);
			schedule_delayed_work(&devp->fw_dwork, 10 * HZ);
		}

		/* workqueue for dvbs blind scan process */
		//INIT_WORK(&devp->blind_scan_work, dvbs_blind_scan_work);
#ifdef AML_DEMOD_SUPPORT_DVBS
		INIT_WORK(&devp->blind_scan_work, dvbs_blind_scan_new_work);
#endif
	}

	demod_attach_register_cb(AM_DTV_DEMOD_AMLDTV, aml_dtvdm_attach);
	PR_INFO("[amldtvdemod.] : version: %s (%s),T2 fw version: %s, probe ok.\n",
			AMLDTVDEMOD_VER, DTVDEMOD_VER, AMLDTVDEMOD_T2_FW_VER);

	return 0;
fail_ic_config:
	PR_ERR("ic config error.\n");
fail_class_create_file:
	PR_ERR("dtvdemod class file create error.\n");
	class_destroy(devp->clsp);
fail_create_class:
	PR_ERR("dtvdemod class create error.\n");
	kfree(devp);
fail_alloc_region:
	PR_ERR("dtvdemod alloc error.\n");
	PR_ERR("dtvdemod_init fail.\n");

	return ret;
}

static int __exit aml_dtvdemod_remove(struct platform_device *pdev)
{
	struct amldtvdemod_device_s *devp =
			(struct amldtvdemod_device_s *)platform_get_drvdata(pdev);

	mutex_lock(&amldtvdemod_device_mutex);

	if (!devp) {
		mutex_unlock(&amldtvdemod_device_mutex);

		return -EFAULT;
	}

	dtvdemod_clktree_remove(&pdev->dev);
	mutex_destroy(&devp->lock);
	dtvdemod_remove_class_files(devp->clsp);
	class_destroy(devp->clsp);

	if (devp->data->hw_ver >= DTVDEMOD_HW_T5D) {
		kfree(devp->fw_buf);
		pm_runtime_put_sync(devp->dev);
		pm_runtime_disable(devp->dev);
	}

	aml_demod_exit();

	list_del_init(&devp->demod_list);

	kfree(devp);

	dtvdd_devp = NULL;

	PR_INFO("%s:remove.\n", __func__);

	mutex_unlock(&amldtvdemod_device_mutex);

	return 0;
}

static void aml_dtvdemod_shutdown(struct platform_device *pdev)
{
	struct amldtvdemod_device_s *devp =
			(struct amldtvdemod_device_s *)platform_get_drvdata(pdev);
	struct aml_dtvdemod *demod = NULL;

	mutex_lock(&amldtvdemod_device_mutex);

	if (!devp) {
		mutex_unlock(&amldtvdemod_device_mutex);

		return;
	}

	mutex_lock(&devp->lock);

	list_for_each_entry(demod, &devp->demod_list, list) {
		/* It will be waked up when it is re-tune.
		 * So it don't have to call the internal resume function.
		 * But need to reinitialize it.
		 */
		if (demod->last_delsys != SYS_UNDEFINED)
			delsys_exit(demod, demod->last_delsys, SYS_UNDEFINED);
	}

	PR_INFO("%s OK.\n", __func__);

	mutex_unlock(&devp->lock);

	mutex_unlock(&amldtvdemod_device_mutex);
}

static int aml_dtvdemod_suspend(struct platform_device *pdev,
					pm_message_t state)
{
	struct amldtvdemod_device_s *devp =
			(struct amldtvdemod_device_s *)platform_get_drvdata(pdev);
	int ret = 0;

	mutex_lock(&amldtvdemod_device_mutex);

	if (!devp) {
		mutex_unlock(&amldtvdemod_device_mutex);

		return -EFAULT;
	}

	mutex_lock(&devp->lock);

	ret = dtvdemod_leave_mode(devp);

	PR_INFO("%s state event %d, ret %d, OK\n", __func__, state.event, ret);

	mutex_unlock(&devp->lock);

	mutex_unlock(&amldtvdemod_device_mutex);

	return 0;
}

static int aml_dtvdemod_resume(struct platform_device *pdev)
{
	PR_INFO("%s is called\n", __func__);

	return 0;
}

static int dtvdemod_leave_mode(struct amldtvdemod_device_s *devp)
{
	enum fe_delivery_system delsys = SYS_UNDEFINED;
	struct aml_dtvdemod *demod = NULL;

	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	list_for_each_entry(demod, &devp->demod_list, list) {
		/* It will be waked up when it is re-tune.
		 * So it don't have to call the internal resume function.
		 * But need to reinitialize it.
		 */
		delsys = demod->last_delsys;
		PR_INFO("%s, delsys = %s\n", __func__, name_fe_delivery_system[delsys]);
		if (delsys != SYS_UNDEFINED) {
#ifdef AML_DEMOD_SUPPORT_DVBS
			if ((delsys == SYS_DVBS || delsys == SYS_DVBS2) &&
					devp->singlecable_param.version)
				PR_INFO("singlecable ODU_poweroff.\n");//TODO
#endif
			delsys_exit(demod, delsys, SYS_UNDEFINED);
		}
	}

	return 0;
}

static __maybe_unused int dtv_demod_pm_suspend(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct amldtvdemod_device_s *devp =
			(struct amldtvdemod_device_s *)platform_get_drvdata(pdev);

	mutex_lock(&amldtvdemod_device_mutex);

	if (!devp) {
		mutex_unlock(&amldtvdemod_device_mutex);

		return -EFAULT;
	}

	mutex_lock(&devp->lock);

	ret = dtvdemod_leave_mode(devp);

	PR_INFO("%s ret %d, OK.\n", __func__, ret);

	mutex_unlock(&devp->lock);

	mutex_unlock(&amldtvdemod_device_mutex);

	return 0;
}

static __maybe_unused int dtv_demod_pm_resume(struct device *dev)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	mutex_lock(&amldtvdemod_device_mutex);

	if (unlikely(!devp)) {
		mutex_unlock(&amldtvdemod_device_mutex);

		return -EFAULT;
	}

	mutex_lock(&devp->lock);

	/* download fw again after STR in case sram was power down */
	devp->fw_wr_done = 0;

	PR_INFO("%s OK.\n", __func__);

	mutex_unlock(&devp->lock);

	mutex_unlock(&amldtvdemod_device_mutex);

	return 0;
}

static int __maybe_unused dtv_demod_runtime_suspend(struct device *dev)
{
	PR_INFO("%s OK.\n", __func__);

	return 0;
}

static int __maybe_unused dtv_demod_runtime_resume(struct device *dev)
{
	PR_INFO("%s OK.\n", __func__);

	return 0;
}

static const struct dev_pm_ops dtv_demod_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(dtv_demod_pm_suspend, dtv_demod_pm_resume)
	SET_RUNTIME_PM_OPS(dtv_demod_runtime_suspend,
			   dtv_demod_runtime_resume, NULL)
};

static struct platform_driver aml_dtvdemod_driver = {
	.driver = {
		.name = "aml_dtv_demod",
		.owner = THIS_MODULE,
		.pm = &dtv_demod_pm_ops,
		/*aml_dtvdemod_dt_match*/
		.of_match_table = meson_ddemod_match,
	},
	.shutdown   = aml_dtvdemod_shutdown,
	.probe = aml_dtvdemod_probe,
	.remove = __exit_p(aml_dtvdemod_remove),
	.suspend  = aml_dtvdemod_suspend,
	.resume   = aml_dtvdemod_resume,
};


int __init aml_dtvdemod_init(void)
{
	if (platform_driver_register(&aml_dtvdemod_driver)) {
		pr_err("failed to register amldtvdemod driver module\n");
		return -ENODEV;
	}

	PR_INFO("[amldtvdemod..]%s.\n", __func__);
	return 0;
}

void __exit aml_dtvdemod_exit(void)
{
	platform_driver_unregister(&aml_dtvdemod_driver);
	PR_INFO("[amldtvdemod..]%s: driver removed ok.\n", __func__);
}

static int delsys_set(struct dvb_frontend *fe, unsigned int delsys)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	enum fe_delivery_system ldelsys = demod->last_delsys;
	enum fe_delivery_system cdelsys = delsys;
	int ncaps = 0, support = 0;
	bool is_T_T2_switch = false;

	if (ldelsys == cdelsys)
		return 0;

	if ((ldelsys == SYS_DVBS && cdelsys == SYS_DVBS2) ||
		(ldelsys == SYS_DVBS2 && cdelsys == SYS_DVBS)) {
		demod->last_delsys = cdelsys;

		return 0;
	}

	if ((cdelsys == SYS_DVBT && ldelsys == SYS_DVBT2) ||
		(cdelsys == SYS_DVBT2 && ldelsys == SYS_DVBT))
		is_T_T2_switch = true;

	while (ncaps < MAX_DELSYS && fe->ops.delsys[ncaps]) {
		if (fe->ops.delsys[ncaps] == cdelsys) {

			support = 1;
			break;
		}
		ncaps++;
	}

	if (!support) {
		PR_INFO("[id %d] delsys:%d is not support!\n", demod->id, cdelsys);
		return 0;
	}

	if (ldelsys <= END_SYS_DELIVERY && cdelsys <= END_SYS_DELIVERY) {
		PR_DBG("%s [id %d]: delsys last=%s, cur=%s\n",
				__func__, demod->id,
				name_fe_delivery_system[ldelsys],
				name_fe_delivery_system[cdelsys]);
	} else
		PR_ERR("%s [id %d]: delsys last=%d, cur=%d\n",
				__func__, demod->id, ldelsys, cdelsys);

	switch (cdelsys) {
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		/* dvbc */
		fe->ops.info.type = FE_QAM;
		break;

	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		/* atsc */
		fe->ops.info.type = FE_ATSC;
		break;

	case SYS_DVBT:
	case SYS_DVBT2:
		/* dvbt, OFDM */
		fe->ops.info.type = FE_OFDM;
		break;

	case SYS_ISDBT:
		fe->ops.info.type = FE_ISDBT;
		break;

	case SYS_DTMB:
		/* dtmb */
		fe->ops.info.type = FE_DTMB;
		break;

	case SYS_DVBS:
	case SYS_DVBS2:
		/* QPSK */
		fe->ops.info.type = FE_QPSK;
		break;

	case SYS_DSS:
	case SYS_DVBH:
	case SYS_ISDBS:
	case SYS_ISDBC:
	case SYS_CMMB:
	case SYS_DAB:
	case SYS_TURBO:
	case SYS_UNDEFINED:
		return 0;
	}

#ifdef CONFIG_AMLOGIC_DVB_COMPAT
	if (cdelsys == SYS_ANALOG) {
		if (get_dtvpll_init_flag()) {
			PR_INFO("delsys not support : %d\n", cdelsys);
			delsys_exit(demod, ldelsys, SYS_UNDEFINED);
		}

		return 0;
	}
#endif

	if (cdelsys != SYS_UNDEFINED) {
		if (ldelsys != SYS_UNDEFINED)
			delsys_exit(demod, ldelsys, cdelsys);

		if (enter_mode(demod, cdelsys)) {
			PR_INFO("enter_mode failed,leave!\n");
			if (demod->inited)
				delsys_exit(demod, cdelsys, SYS_UNDEFINED);

			return 0;
		}
	}

	if (!get_dtvpll_init_flag()) {
		PR_INFO("pll is not set!\n");
		delsys_exit(demod, cdelsys, SYS_UNDEFINED);

		return 0;
	}

	demod->last_delsys = cdelsys;
	PR_INFO("[id %d] info fe type:%d.\n", demod->id, fe->ops.info.type);

	if (fe->ops.tuner_ops.set_config && !is_T_T2_switch)
		fe->ops.tuner_ops.set_config(fe, NULL);

	return 0;
}

static int is_not_active(struct dvb_frontend *fe)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	enum fe_delivery_system cdelsys = fe->dtv_property_cache.delivery_system;
	enum fe_delivery_system ldelsys = demod->last_delsys;

	if (!get_dtvpll_init_flag()) {
		PR_INFO("%s: [id %d] dtvpll uninit.\n", __func__, demod->id);

		return 1;
	}

	if (ldelsys == SYS_UNDEFINED) {
		PR_INFO("%s: [id %d] ldelsys == SYS_UNDEFINED.\n",
			__func__, demod->id);

		return 2;
	}

	if (ldelsys != cdelsys) {
		PR_INFO("%s: [id %d] ldelsys(%d) != cdelsys(%d).\n",
			__func__, demod->id, ldelsys, cdelsys);

		return 3;
	}

	if (!devp->demod_thread) {
		PR_INFO("%s: [id %d] devp->demod_thread(%d).\n",
			__func__, demod->id, devp->demod_thread);

		return 4;
	}

	return 0;/*active*/
}

static int aml_dtvdm_init(struct dvb_frontend *fe)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;

	mutex_lock(&devp->lock);

	demod->suspended = false;
	demod->last_delsys = SYS_UNDEFINED;
	fe->ops.info.type = 0xFF; /* undefined */

	PR_INFO("%s [id %d] OK.\n", __func__, demod->id);

	mutex_unlock(&devp->lock);

	return 0;
}

static int aml_dtvdm_sleep(struct dvb_frontend *fe)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	enum fe_delivery_system delsys = SYS_UNDEFINED;

	mutex_lock(&devp->lock);

	delsys = demod->last_delsys;

	if (get_dtvpll_init_flag()) {
		PR_INFO("%s\n", __func__);

		if (delsys != SYS_UNDEFINED)
			delsys_exit(demod, delsys, SYS_UNDEFINED);
	}

	PR_INFO("%s [id %d] OK.\n", __func__, demod->id);

	mutex_unlock(&devp->lock);

	return 0;
}

static int aml_dtvdm_set_parameters(struct dvb_frontend *fe)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	enum fe_delivery_system delsys = SYS_UNDEFINED;
	int ret = 0;
	struct dtv_frontend_properties *c = NULL;

	mutex_lock(&devp->lock);

	delsys = demod->last_delsys;
	c = &fe->dtv_property_cache;

	PR_DBGL("%s [id %d] delsys %d %s.\n", __func__,
			demod->id, delsys, name_fe_delivery_system[delsys]);
	PR_DBGL("[id %d] delsys=%d freq=%d,symbol_rate=%d,bw=%d,modulation=%d,inversion=%d.\n",
		demod->id, c->delivery_system, c->frequency,
		c->symbol_rate, c->bandwidth_hz, c->modulation,
		c->inversion);

	if (is_not_active(fe)) {
		mutex_unlock(&devp->lock);

		return -ECANCELED;
	}

	switch (delsys) {
	case SYS_DVBS:
	case SYS_DVBS2:
		break;
#ifdef AML_DEMOD_SUPPORT_DVBC
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		timer_begain(demod, D_TIMER_DETECT);
		demod->en_detect = 1; /*fist set*/
		ret = gxtv_demod_dvbc_set_frontend(fe);
		break;
#endif
	case SYS_DVBT:
	case SYS_DVBT2:
		break;
#ifdef AML_DEMOD_SUPPORT_ISDBT
	case SYS_ISDBT:
		timer_begain(demod, D_TIMER_DETECT);
		demod->en_detect = 1; /*fist set*/
		ret = dvbt_isdbt_set_frontend(fe);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_ATSC
	case SYS_ATSC:
	case SYS_ATSCMH:
		ret = gxtv_demod_atsc_set_frontend(fe);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_J83B
	case SYS_DVBC_ANNEX_B:
		ret = gxtv_demod_atsc_j83b_set_frontend(fe);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_DTMB
	case SYS_DTMB:
		ret = gxtv_demod_dtmb_set_frontend(fe);
		break;
#endif
	case SYS_UNDEFINED:
	default:
		break;
	}

	mutex_unlock(&devp->lock);

	return ret;
}

static int aml_dtvdm_get_frontend(struct dvb_frontend *fe,
				 struct dtv_frontend_properties *p)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	enum fe_delivery_system delsys = SYS_UNDEFINED;
	int ret = 0;

	mutex_lock(&devp->lock);

	delsys = demod->last_delsys;

	if (is_not_active(fe)) {
		mutex_unlock(&devp->lock);

		return -ECANCELED;
	}

	switch (delsys) {
#ifdef AML_DEMOD_SUPPORT_DVBS
	case SYS_DVBS:
	case SYS_DVBS2:
		if (!devp->blind_scan_stop) {
			p->frequency = demod->blind_result_frequency;
			p->symbol_rate = demod->blind_result_symbol_rate;
			p->delivery_system = delsys;
		} else {
			p->frequency = fe->dtv_property_cache.frequency;
			p->symbol_rate = fe->dtv_property_cache.symbol_rate;
			p->delivery_system = delsys;
		}

		PR_DVBS("%s [id %d] delsys %d,freq %d,srate %d\n",
				__func__, demod->id, delsys,
				p->frequency, p->symbol_rate);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_DVBC
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		ret = gxtv_demod_dvbc_get_frontend(fe);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_DVBT
	case SYS_DVBT:
	case SYS_DVBT2:
		ret = gxtv_demod_dvbt_get_frontend(fe);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_ISDBT
	case SYS_ISDBT:
		ret = gxtv_demod_isdbt_get_frontend(fe);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_ATSC
	case SYS_ATSC:
	case SYS_ATSCMH:
		ret = gxtv_demod_atsc_get_frontend(fe);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_J83B
	case SYS_DVBC_ANNEX_B:
		ret = gxtv_demod_atsc_j83b_get_frontend(fe);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_DTMB
	case SYS_DTMB:
		ret = gxtv_demod_dtmb_get_frontend(fe);
		break;
#endif
	case SYS_UNDEFINED:
	default:
		break;
	}

	mutex_unlock(&devp->lock);

	return ret;
}

static int aml_dtvdm_get_tune_settings(struct dvb_frontend *fe,
		struct dvb_frontend_tune_settings *fe_tune_settings)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	enum fe_delivery_system delsys = SYS_UNDEFINED;
	int ret = 0;

	mutex_lock(&devp->lock);

	delsys = demod->last_delsys;

	if (is_not_active(fe)) {
		mutex_unlock(&devp->lock);

		return -ECANCELED;
	}

	switch (delsys) {
	case SYS_DVBS:
	case SYS_DVBS2:
		break;

	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		fe_tune_settings->min_delay_ms = 300;
		fe_tune_settings->step_size = 0; /* no zigzag */
		fe_tune_settings->max_drift = 0;
		break;

	case SYS_DVBT:
	case SYS_DVBT2:
		fe_tune_settings->min_delay_ms = 500;
		fe_tune_settings->step_size = 0;
		fe_tune_settings->max_drift = 0;
		break;

	case SYS_ISDBT:
		fe_tune_settings->min_delay_ms = 300;
		fe_tune_settings->step_size = 0;
		fe_tune_settings->max_drift = 0;
		break;

	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		break;

	case SYS_DTMB:
		break;

	case SYS_UNDEFINED:
	default:
		break;
	}

	mutex_unlock(&devp->lock);

	return ret;
}

static int aml_dtvdm_read_status(struct dvb_frontend *fe,
		enum fe_status *status)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	enum fe_delivery_system delsys = SYS_UNDEFINED;
	int ret = 0;

	mutex_lock(&devp->lock);

	delsys = demod->last_delsys;

	if (is_not_active(fe)) {
		mutex_unlock(&devp->lock);

		return -ECANCELED;
	}

	switch (delsys) {
	case SYS_DVBS:
	case SYS_DVBS2:
		*status = demod->last_status;
		break;
#ifdef AML_DEMOD_SUPPORT_DVBC
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		if (dvbc_new_driver)
			*status = demod->last_status;
		else
			ret = gxtv_demod_dvbc_read_status_timer(fe, status);
		break;
#endif
	case SYS_DVBT:
		*status = demod->last_status;
		break;

	case SYS_DVBT2:
		*status = demod->last_status;
		break;

	case SYS_ISDBT:
		*status = demod->last_status;
		break;

	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		*status = demod->last_status;
		break;
#ifdef AML_DEMOD_SUPPORT_DTMB
	case SYS_DTMB:
		ret = gxtv_demod_dtmb_read_status(fe, status);
		break;
#endif
	default:
		break;
	}

	PR_DBG("%s: [id %d]: delsys %d, status 0x%x, ret %d.\n",
			__func__, demod->id, delsys, *status, ret);

	mutex_unlock(&devp->lock);

	return ret;
}

static int aml_dtvdm_read_ber(struct dvb_frontend *fe, u32 *ber)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	enum fe_delivery_system delsys = SYS_UNDEFINED;
	int ret = 0;

	mutex_lock(&devp->lock);

	delsys = demod->last_delsys;

	if (is_not_active(fe)) {
		mutex_unlock(&devp->lock);

		return -ECANCELED;
	}

	switch (delsys) {
#ifdef AML_DEMOD_SUPPORT_DVBS
	case SYS_DVBS:
	case SYS_DVBS2:
		ret = dtvdemod_dvbs_read_ber(fe, ber);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_DVBC
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		ret = gxtv_demod_dvbc_read_ber(fe, ber);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_DVBT
	case SYS_DVBT:
		ret = dvbt_read_ber(fe, ber);
		break;

	case SYS_DVBT2:
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_ISDBT
	case SYS_ISDBT:
		ret = dvbt_isdbt_read_ber(fe, ber);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_ATSC
	case SYS_ATSC:
	case SYS_ATSCMH:
		ret = gxtv_demod_atsc_read_ber(fe, ber);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_J83B
	case SYS_DVBC_ANNEX_B:
		ret = gxtv_demod_atsc_j83b_read_ber(fe, ber);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_DTMB
	case SYS_DTMB:
		ret = gxtv_demod_dtmb_read_ber(fe, ber);
		break;
#endif
	default:
		break;
	}

	mutex_unlock(&devp->lock);

	return ret;
}

static int aml_dtvdm_read_signal_strength(struct dvb_frontend *fe,
		u16 *strength)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	enum fe_delivery_system delsys = SYS_UNDEFINED;
	int ret = 0;

	mutex_lock(&devp->lock);

	delsys = demod->last_delsys;

	if (is_not_active(fe)) {
		mutex_unlock(&devp->lock);

		return -ECANCELED;
	}

	switch (delsys) {
#ifdef AML_DEMOD_SUPPORT_DVBS
	case SYS_DVBS:
	case SYS_DVBS2:
		ret = dtvdemod_dvbs_read_signal_strength(fe, (s16 *)strength);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_DVBC
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		ret = gxtv_demod_dvbc_read_signal_strength(fe, (s16 *)strength);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_DVBT
	case SYS_DVBT:
	case SYS_DVBT2:
		ret = gxtv_demod_dvbt_read_signal_strength(fe, (s16 *)strength);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_ISDBT
	case SYS_ISDBT:
		ret = gxtv_demod_isdbt_read_signal_strength(fe, (s16 *)strength);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_ATSC
	case SYS_ATSC:
	case SYS_ATSCMH:
		ret = gxtv_demod_atsc_read_signal_strength(fe, (s16 *)strength);//ok
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_J83B
	case SYS_DVBC_ANNEX_B:
		ret = gxtv_demod_atsc_j83b_read_signal_strength(fe, (s16 *)strength);//ok
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_DTMB
	case SYS_DTMB:
		ret = gxtv_demod_dtmb_read_signal_strength(fe, (s16 *)strength);
		break;
#endif
	default:
		break;
	}

	mutex_unlock(&devp->lock);

	return ret;
}

static int aml_dtvdm_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	enum fe_delivery_system delsys = SYS_UNDEFINED;
	int ret = 0;

	mutex_lock(&devp->lock);

	delsys = demod->last_delsys;

	if (is_not_active(fe)) {
		mutex_unlock(&devp->lock);

		return -ECANCELED;
	}

	switch (delsys) {
#ifdef AML_DEMOD_SUPPORT_DVBS
	case SYS_DVBS:
	case SYS_DVBS2:
		ret = dvbs_read_snr(fe, snr);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_DVBC
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		ret = gxtv_demod_dvbc_read_snr(fe, snr);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_DVBT
	case SYS_DVBT:
		ret = dvbt_read_snr(fe, snr);
		break;

	case SYS_DVBT2:
		ret = dvbt2_read_snr(fe, snr);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_ISDBT
	case SYS_ISDBT:
		ret = gxtv_demod_dvbt_isdbt_read_snr(fe, snr);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_ATSC
	case SYS_ATSC:
	case SYS_ATSCMH:
		ret = gxtv_demod_atsc_read_snr(fe, snr);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_J83B
	case SYS_DVBC_ANNEX_B:
		ret = gxtv_demod_atsc_j83b_read_snr(fe, snr);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_DTMB
	case SYS_DTMB:
		ret = gxtv_demod_dtmb_read_snr(fe, snr);
		break;
#endif
	default:
		break;
	}

	mutex_unlock(&devp->lock);

	return ret;
}

static int aml_dtvdm_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	enum fe_delivery_system delsys = SYS_UNDEFINED;
	int ret = 0;

	mutex_lock(&devp->lock);

	delsys = demod->last_delsys;

	if (is_not_active(fe)) {
		mutex_unlock(&devp->lock);

		return -ECANCELED;
	}

	switch (delsys) {
	case SYS_DVBS:
	case SYS_DVBS2:
		break;
#ifdef AML_DEMOD_SUPPORT_DVBC
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		ret = gxtv_demod_dvbc_read_ucblocks(fe, ucblocks);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_DVBT
	case SYS_DVBT:
	case SYS_DVBT2:
		ret = gxtv_demod_dvbt_read_ucblocks(fe, ucblocks);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_ISDBT
	case SYS_ISDBT:
		ret = gxtv_demod_isdbt_read_ucblocks(fe, ucblocks);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_ATSC
	case SYS_ATSC:
	case SYS_ATSCMH:
		ret = gxtv_demod_atsc_read_ucblocks(fe, ucblocks);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_J83B
	case SYS_DVBC_ANNEX_B:
		ret = gxtv_demod_atsc_j83b_read_ucblocks(fe, ucblocks);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_DTMB
	case SYS_DTMB:
		ret = gxtv_demod_dtmb_read_ucblocks(fe, ucblocks);
		break;
#endif
	default:
		break;
	}

	mutex_unlock(&devp->lock);

	return ret;
}

static void aml_dtvdm_release(struct dvb_frontend *fe)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	enum fe_delivery_system delsys = SYS_UNDEFINED;

	mutex_lock(&devp->lock);

	delsys = demod->last_delsys;

	switch (delsys) {
	case SYS_DVBS:
	case SYS_DVBS2:
		break;
#ifdef AML_DEMOD_SUPPORT_DVBC
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		gxtv_demod_dvbc_release(fe);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_DVBT
	case SYS_DVBT:
	case SYS_DVBT2:
		break;
#endif
	case SYS_ISDBT:
		break;
#ifdef AML_DEMOD_SUPPORT_ATSC
	case SYS_ATSC:
	case SYS_ATSCMH:
		gxtv_demod_atsc_release(fe);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_J83B
	case SYS_DVBC_ANNEX_B:
		gxtv_demod_atsc_j83b_release(fe);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_DTMB
	case SYS_DTMB:
		gxtv_demod_dtmb_release(fe);
		break;
#endif
	default:
		break;
	}

	if (get_dtvpll_init_flag()) {
		PR_INFO("%s\n", __func__);
		if (delsys != SYS_UNDEFINED)
			delsys_exit(demod, delsys, SYS_UNDEFINED);
	}

	PR_INFO("%s [id %d] OK.\n", __func__, demod->id);

	mutex_unlock(&devp->lock);
}

static int aml_dtvdm_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	enum fe_delivery_system delsys = SYS_UNDEFINED;
	int ret = 0;
	static int flg; /*debug only*/

	mutex_lock(&devp->lock);

	delsys = demod->last_delsys;

	if (delsys == SYS_UNDEFINED) {
		*delay = HZ * 5;
		*status = 0;
		mutex_unlock(&devp->lock);
		return 0;
	}

	if (is_not_active(fe)) {
		*status = 0;
		mutex_unlock(&devp->lock);

		return -ECANCELED;
	}

	if ((flg > 0) && (flg < 5))
		PR_INFO("%s [id %d]\n", __func__, demod->id);

	switch (delsys) {
#ifdef AML_DEMOD_SUPPORT_DVBS
	case SYS_DVBS:
	case SYS_DVBS2:
		ret = dtvdemod_dvbs_tune(fe, re_tune, mode_flags, delay, status);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_DVBC
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		if (dvbc_new_driver)
			dvbc_tune(fe, re_tune, mode_flags, delay, status);
		else
			gxtv_demod_dvbc_tune(fe, re_tune, mode_flags, delay, status);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_DVBT
	case SYS_DVBT:
		ret = dvbt_tune(fe, re_tune, mode_flags, delay, status);
		break;

	case SYS_DVBT2:
		ret = dvbt2_tune(fe, re_tune, mode_flags, delay, status);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_ISDBT
	case SYS_ISDBT:
		ret = dvbt_isdbt_tune(fe, re_tune, mode_flags, delay, status);
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_ATSC
	case SYS_ATSC:
	case SYS_ATSCMH:
		ret = gxtv_demod_atsc_tune(fe, re_tune, mode_flags, delay, status);
		flg++;
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_J83B
	case SYS_DVBC_ANNEX_B:
		ret = gxtv_demod_atsc_j83b_tune(fe, re_tune, mode_flags, delay, status);
		flg++;
		break;
#endif
#ifdef AML_DEMOD_SUPPORT_DTMB
	case SYS_DTMB:
		ret = gxtv_demod_dtmb_tune(fe, re_tune, mode_flags, delay, status);
		break;
#endif
	default:
		flg = 0;
		break;
	}

	mutex_unlock(&devp->lock);

	return ret;
}

static int aml_dtvdm_set_property(struct dvb_frontend *fe,
			struct dtv_property *tvp)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
	int r = 0;
	u32 delsys = SYS_UNDEFINED;

	mutex_lock(&devp->lock);
	PR_DBG("%s: cmd %d %d\n", __func__, tvp->cmd, tvp->u.data);

	if (is_not_active(fe) && tvp->cmd != DTV_DELIVERY_SYSTEM) {
		mutex_unlock(&devp->lock);

		return -ECANCELED;
	}

	switch (tvp->cmd) {
	case DTV_DELIVERY_SYSTEM:
		delsys = tvp->u.data;
		delsys_set(fe, delsys);
		break;

	case DTV_DVBT2_PLP_ID:
		demod->plp_id = tvp->u.data;
		PR_INFO("[id %d] DTV_DVBT2_PLP_ID, %d\n", demod->id, demod->plp_id);
		break;

	case DTV_BLIND_SCAN_MIN_FRE:
		devp->blind_min_fre = tvp->u.data;
		PR_INFO("DTV_BLIND_SCAN_MIN_FRE: %d\n", devp->blind_min_fre);
		break;

	case DTV_BLIND_SCAN_MAX_FRE:
		devp->blind_max_fre = tvp->u.data;
		PR_INFO("DTV_BLIND_SCAN_MAX_FRE: %d\n", devp->blind_max_fre);
		break;

	case DTV_BLIND_SCAN_MIN_SRATE:
		devp->blind_min_srate = tvp->u.data;
		PR_INFO("DTV_BLIND_SCAN_MIN_SRATE: %d\n", devp->blind_min_srate);
		break;

	case DTV_BLIND_SCAN_MAX_SRATE:
		devp->blind_max_srate = tvp->u.data;
		PR_INFO("DTV_BLIND_SCAN_MAX_SRATE: %d\n", devp->blind_max_srate);
		break;

	case DTV_BLIND_SCAN_FRE_RANGE:
		devp->blind_fre_range = tvp->u.data;
		PR_INFO("DTV_BLIND_SCAN_FRE_RANGE: %d\n", devp->blind_fre_range);
		break;

	case DTV_BLIND_SCAN_FRE_STEP:
		//devp->blind_fre_step = tvp->u.data;
		//set blind scan setp fft for 40M
		devp->blind_fre_step = 40000;

		if (!devp->blind_fre_step)
			devp->blind_fre_step = 2000;/* 2M */

		PR_INFO("DTV_BLIND_SCAN_FRE_STEP: %d\n", devp->blind_fre_step);
		break;

	case DTV_BLIND_SCAN_TIMEOUT:
		devp->blind_timeout = tvp->u.data;
		PR_INFO("DTV_BLIND_SCAN_TIMEOUT: %d\n", devp->blind_timeout);
		break;

	case DTV_START_BLIND_SCAN:
		if (!devp->blind_scan_stop) {
			PR_INFO("blind_scan already started\n");
			break;
		}
		PR_INFO("DTV_START_BLIND_SCAN\n");
		devp->blind_scan_stop = 0;
		schedule_work(&devp->blind_scan_work);
		PR_INFO("schedule workqueue for blind scan, return\n");
		break;

	case DTV_CANCEL_BLIND_SCAN:
		devp->blind_scan_stop = 1;
		PR_INFO("DTV_CANCEL_BLIND_SCAN\n");
		/* Normally, need to call cancel_work_sync()
		 * wait to workqueue exit,
		 * but this will cause a deadlock.
		 */
		break;
	case DTV_SINGLE_CABLE_VER:
		/* not singlecable: 0, 1.0X - 1(EN50494), 2.0X - 2(EN50607) */
		if (devp->singlecable_param.version && !tvp->u.data)
			PR_INFO("singlecable ODU_poweroff.\n");//TODO

		devp->singlecable_param.version = tvp->u.data;
		break;
	case DTV_SINGLE_CABLE_USER_BAND:
		//user band (1-8)
		devp->singlecable_param.userband = tvp->u.data + 1;
		break;
	case DTV_SINGLE_CABLE_BAND_FRE:
		devp->singlecable_param.frequency = tvp->u.data;
		break;
	case DTV_SINGLE_CABLE_BANK:
		devp->singlecable_param.bank = tvp->u.data;
		break;
	case DTV_SINGLE_CABLE_UNCOMMITTED:
		devp->singlecable_param.uncommitted = tvp->u.data;
		break;
	case DTV_SINGLE_CABLE_COMMITTED:
		devp->singlecable_param.committed = tvp->u.data;
		break;
	default:
		break;
	}

	mutex_unlock(&devp->lock);

	return r;
}

static int aml_dtvdm_get_property(struct dvb_frontend *fe,
			struct dtv_property *tvp)
{
#ifdef AML_DEMOD_SUPPORT_DVBS
	char v;
#endif
	unsigned char modulation = 0xFF;
	unsigned char cr = 0xFF;
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	struct amldtvdemod_device_s *devp = (struct amldtvdemod_device_s *)demod->priv;
#ifdef AML_DEMOD_SUPPORT_ISDBT
	struct isdbt_tmcc_info tmcc_info;
#endif

	mutex_lock(&devp->lock);

	if (is_not_active(fe) && tvp->cmd != DTV_TS_INPUT &&
		tvp->cmd != DTV_ENUM_DELSYS) {
		mutex_unlock(&devp->lock);

		return -ECANCELED;
	}

	switch (tvp->cmd) {
	case DTV_DELIVERY_SYSTEM:
		tvp->u.data = demod->last_delsys;
		if (demod->last_delsys == SYS_DVBS || demod->last_delsys == SYS_DVBS2) {
#ifdef AML_DEMOD_SUPPORT_DVBS
			v = dvbs_rd_byte(0x932) & 0x60;//bit5.6
			modulation = dvbs_rd_byte(0x930) >> 2;
			if (v == 0x40) {//bit6=1.bit5=0 means S2
				tvp->u.data = SYS_DVBS2;
				if ((modulation >= 0x0c && modulation <= 0x11) ||
					(modulation >= 0x47 && modulation <= 0x49))
					tvp->reserved[0] = PSK_8;
				else if ((modulation >= 0x12 && modulation <= 0x17) ||
					(modulation >= 0x4a && modulation <= 0x56))
					tvp->reserved[0] = APSK_16;
				else if ((modulation >= 0x18 && modulation <= 0x1c) ||
					(modulation >= 0x57 && modulation <= 0x59))
					tvp->reserved[0] = APSK_32;
				else
					tvp->reserved[0] = QPSK;
			} else if (v == 0x60) {//bit6=1.bit5=1 means S
				tvp->u.data = SYS_DVBS;
				tvp->reserved[0] = QPSK;
			}

			PR_DVBS("[id %d] get delsys:%d,modulation:%d.\n",
					demod->id, tvp->u.data, tvp->reserved[0]);
#endif
		} else if (demod->last_delsys == SYS_DVBT2 &&
			demod->last_status == 0x1F) {
			modulation = demod->real_para.modulation;
			cr = demod->real_para.coderate;

			if (modulation == 0)
				tvp->reserved[0] = QPSK;
			else if (modulation == 1)
				tvp->reserved[0] = QAM_16;
			else if (modulation == 2)
				tvp->reserved[0] = QAM_64;
			else if (modulation == 3)
				tvp->reserved[0] = QAM_256;
			else
				tvp->reserved[0] = 0xFF;

			if (cr == 0)
				tvp->reserved[1] = FEC_1_2;
			else if (cr == 1)
				tvp->reserved[1] = FEC_3_5;
			else if (cr == 2)
				tvp->reserved[1] = FEC_2_3;
			else if (cr == 3)
				tvp->reserved[1] = FEC_3_4;
			else if (cr == 4)
				tvp->reserved[1] = FEC_4_5;
			else if (cr == 5)
				tvp->reserved[1] = FEC_5_6;
			else
				tvp->reserved[1] = 0xFF;

			tvp->reserved[2] = demod->real_para.fef_info;

			PR_DVBT("[id %d] get delsys:%d,modulation:%d,code_rate:%d,fef_info:%d.\n",
				demod->id, tvp->u.data, tvp->reserved[0],
				tvp->reserved[1], tvp->reserved[2]);

		} else if (demod->last_delsys == SYS_DVBT &&
			demod->last_status == 0x1F) {
			modulation = demod->real_para.modulation;
			cr = demod->real_para.coderate;

			if (modulation == 0)
				tvp->reserved[0] = QPSK;
			else if (modulation == 1)
				tvp->reserved[0] = QAM_16;
			else if (modulation == 2)
				tvp->reserved[0] = QAM_64;
			else
				tvp->reserved[0] = 0xFF;

			if (cr == 0)
				tvp->reserved[1] = FEC_1_2;
			else if (cr == 1)
				tvp->reserved[1] = FEC_2_3;
			else if (cr == 2)
				tvp->reserved[1] = FEC_3_4;
			else if (cr == 3)
				tvp->reserved[1] = FEC_5_6;
			else if (cr == 4)
				tvp->reserved[1] = FEC_7_8;
			else
				tvp->reserved[1] = 0xFF;

			tvp->reserved[2] = demod->real_para.tps_cell_id;

			PR_DVBT("[id %d] get delsys:%d,modulation:%d,code_rate:%d,cell_id:%d.\n",
					demod->id, tvp->u.data,
					tvp->reserved[0], tvp->reserved[1], tvp->reserved[2]);
		} else if ((demod->last_delsys == SYS_DVBC_ANNEX_A ||
			demod->last_delsys == SYS_DVBC_ANNEX_C) &&
			demod->last_status == 0x1F) {
			tvp->reserved[0] = demod->real_para.modulation;
			tvp->reserved[1] = demod->real_para.symbol;

			PR_DVBC("[id %d] get delsys:%d,modulation:%d,symbol:%d.\n",
				demod->id, tvp->u.data, tvp->reserved[0], tvp->reserved[1]);
		}
		break;

	case DTV_DVBT2_PLP_ID:
		/* plp nums & ids */
		tvp->u.buffer.reserved1[0] = demod->real_para.plp_num;
		if (tvp->u.buffer.reserved2 && demod->real_para.plp_num > 0) {
			unsigned char i, *plp_ids;

			plp_ids = kmalloc(demod->real_para.plp_num, GFP_KERNEL);
			if (plp_ids) {
				for (i = 0; i < demod->real_para.plp_num; i++)
					plp_ids[i] = i;
				if (copy_to_user(tvp->u.buffer.reserved2,
					plp_ids, demod->real_para.plp_num))
					PR_ERR("copy plp ids to user err\n");

				kfree(plp_ids);
			}
		}
		PR_INFO("[id %d] get plp num = %d\n",
			demod->id, demod->real_para.plp_num);
		break;

	case DTV_STAT_CNR:
		if (demod->last_delsys == SYS_DVBS || demod->last_delsys == SYS_DVBS2)
			tvp->reserved[0] = dvbs_rd_byte(0xd12) / 10;

		PR_DVBS("[id %d] get delsys:%d,cnr:%d.\n",
				demod->id, tvp->u.data, tvp->reserved[0]);
		break;

	case DTV_TS_INPUT:
		if (is_meson_s4d_cpu() || is_meson_s4_cpu() || is_meson_s1a_cpu())
			tvp->u.data = demod->id + 1; // tsin1 and tsin2.
		else if (is_meson_t3x_cpu())
			tvp->u.data = 3; // tsin3.
		else
			tvp->u.data = 2; // tsin2.
		break;
#ifdef AML_DEMOD_SUPPORT_ISDBT
	case DTV_ISDBT_PARTIAL_RECEPTION:
		if (demod->last_delsys == SYS_ISDBT && demod->last_status == 0x1F) {
			isdbt_get_tmcc_info(&tmcc_info);

			tvp->u.buffer.reserved1[0] = tmcc_info.system_id;
			tvp->u.buffer.reserved1[1] = tmcc_info.ews_flag;
			tvp->u.buffer.reserved1[2] = tmcc_info.current_info.is_partial;
		}
		break;
#endif
	default:
		break;
	}

	mutex_unlock(&devp->lock);

	return 0;
}

static struct dvb_frontend_ops aml_dtvdm_ops = {
	.delsys = {SYS_UNDEFINED},
	.info = {
		/*in aml_fe, it is 'amlogic dvb frontend' */
		.name = "",
		.caps = FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
			FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_64 |
			FE_CAN_QAM_AUTO | FE_CAN_TRANSMISSION_MODE_AUTO |
			FE_CAN_GUARD_INTERVAL_AUTO | FE_CAN_HIERARCHY_AUTO |
			FE_CAN_RECOVER | FE_CAN_MUTE_TS
	},
	.init = aml_dtvdm_init,
	.sleep = aml_dtvdm_sleep,
	.set_frontend = aml_dtvdm_set_parameters,
	.get_frontend = aml_dtvdm_get_frontend,
	.get_tune_settings = aml_dtvdm_get_tune_settings,
	.read_status = aml_dtvdm_read_status,
	.read_ber = aml_dtvdm_read_ber,
	.read_signal_strength = aml_dtvdm_read_signal_strength,
	.read_snr = aml_dtvdm_read_snr,
	.read_ucblocks = aml_dtvdm_read_ucblocks,
	.release = aml_dtvdm_release,
	.set_property = aml_dtvdm_set_property,
	.get_property = aml_dtvdm_get_property,
	.tune = aml_dtvdm_tune,
	.get_frontend_algo = gxtv_demod_get_frontend_algo,
};

struct dvb_frontend *aml_dtvdm_attach(const struct demod_config *config)
{
	int ic_version = get_cpu_type();
	unsigned int ic_is_supportted = 1;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();
	struct aml_dtvdemod *demod = NULL;

	if (IS_ERR_OR_NULL(devp)) {
		pr_err("%s: error devp is NULL\n", __func__);

		return NULL;
	}

	mutex_lock(&amldtvdemod_device_mutex);

	if ((devp->data->hw_ver != DTVDEMOD_HW_S4 &&
		devp->data->hw_ver != DTVDEMOD_HW_S4D && devp->index > 0) ||
		(devp->data->hw_ver == DTVDEMOD_HW_S4 && devp->index > 1) ||
		(devp->data->hw_ver == DTVDEMOD_HW_S4D && devp->index > 1)) {
		pr_err("%s: Had attached (%d), only S4 and S4D support 2 attach.\n",
				__func__, devp->index);

		mutex_unlock(&amldtvdemod_device_mutex);

		return NULL;
	}

	demod = kzalloc(sizeof(*demod), GFP_KERNEL);
	if (!demod) {
		pr_err("%s: kzalloc for demod fail.\n", __func__);

		mutex_unlock(&amldtvdemod_device_mutex);

		return NULL;
	}

	INIT_LIST_HEAD(&demod->list);

	demod->id = devp->index;
	demod->act_dtmb = false;
	demod->timeout_atsc_ms = TIMEOUT_ATSC;
	demod->timeout_dvbt_ms = TIMEOUT_DVBT;
	demod->timeout_dvbs_ms = TIMEOUT_DVBS;
	demod->timeout_dvbc_ms = TIMEOUT_DVBC;
	demod->timeout_ddr_leave = TIMEOUT_DDR_LEAVE;
	demod->last_qam_mode = QAM_MODE_NUM;
	demod->last_lock = -1;
	demod->inited = false;
	demod->suspended = true;

	/* select dvbc module for s4 and S4D */
	if (devp->data->hw_ver == DTVDEMOD_HW_S4 ||
		devp->data->hw_ver == DTVDEMOD_HW_S4D)
		demod->dvbc_sel = demod->id;
	else
		demod->dvbc_sel = 0;

	demod->priv = devp;
	demod->frontend.demodulator_priv = demod;
	demod->last_delsys = SYS_UNDEFINED;

	switch (ic_version) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	case MESON_CPU_MAJOR_ID_GXTVBB:
	case MESON_CPU_MAJOR_ID_TXL:
		aml_dtvdm_ops.delsys[0] = SYS_DVBC_ANNEX_A;
		aml_dtvdm_ops.delsys[1] = SYS_DTMB;
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
		aml_dtvdm_ops.delsys[2] = SYS_ANALOG;
#endif
		if (ic_version == MESON_CPU_MAJOR_ID_GXTVBB)
			strcpy(aml_dtvdm_ops.info.name, "amlogic DVB-C/DTMB dtv demod gxtvbb");
		else
			strcpy(aml_dtvdm_ops.info.name, "amlogic DVB-C/DTMB dtv demod txl");
		break;

	case MESON_CPU_MAJOR_ID_TXLX:
		aml_dtvdm_ops.delsys[0] = SYS_ATSC;
		aml_dtvdm_ops.delsys[1] = SYS_DVBC_ANNEX_B;
		aml_dtvdm_ops.delsys[2] = SYS_DVBC_ANNEX_A;
		aml_dtvdm_ops.delsys[3] = SYS_DVBT;
		aml_dtvdm_ops.delsys[4] = SYS_ISDBT;
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
		aml_dtvdm_ops.delsys[5] = SYS_ANALOG;
#endif
		strcpy(aml_dtvdm_ops.info.name, "amlogic DVB-C/T/ISDBT/ATSC dtv demod txlx");
		break;

	case MESON_CPU_MAJOR_ID_GXLX:
		aml_dtvdm_ops.delsys[0] = SYS_DVBC_ANNEX_A;
		strcpy(aml_dtvdm_ops.info.name, "amlogic DVBC dtv demod gxlx");
		break;

	case MESON_CPU_MAJOR_ID_TXHD:
		aml_dtvdm_ops.delsys[0] = SYS_DTMB;
		strcpy(aml_dtvdm_ops.info.name, "amlogic DTMB dtv demod txhd");
		break;

	case MESON_CPU_MAJOR_ID_TL1:
#endif
	case MESON_CPU_MAJOR_ID_TM2:
		aml_dtvdm_ops.delsys[0] = SYS_DVBC_ANNEX_A;
		aml_dtvdm_ops.delsys[1] = SYS_DVBC_ANNEX_B;
		aml_dtvdm_ops.delsys[2] = SYS_ATSC;
		aml_dtvdm_ops.delsys[3] = SYS_DTMB;
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
		aml_dtvdm_ops.delsys[4] = SYS_ANALOG;
#endif
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		if (ic_version == MESON_CPU_MAJOR_ID_TL1) {
			strcpy(aml_dtvdm_ops.info.name, "amlogic DVB-C/DTMB/ATSC dtv demod tl1");
		} else {
#else
		{
#endif
			strcpy(aml_dtvdm_ops.info.name, "amlogic DVB-C/DTMB/ATSC dtv demod tm2");
		}
		break;

	case MESON_CPU_MAJOR_ID_T5:
		/* max delsys is 8, index: 0~7 */
		aml_dtvdm_ops.delsys[0] = SYS_DVBC_ANNEX_A;
		aml_dtvdm_ops.delsys[1] = SYS_DTMB;
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
		aml_dtvdm_ops.delsys[2] = SYS_ANALOG;
#endif
		strcpy(aml_dtvdm_ops.info.name, "amlogic DVB-C/DTMB dtv demod t5");
		break;

	default:
		switch (devp->data->hw_ver) {
		case DTVDEMOD_HW_T5D:
		case DTVDEMOD_HW_T5D_B:
			/* max delsys is 8, index: 0~7 */
			aml_dtvdm_ops.delsys[0] = SYS_DVBC_ANNEX_A;
			aml_dtvdm_ops.delsys[1] = SYS_ATSC;
			aml_dtvdm_ops.delsys[2] = SYS_DVBS2;
			aml_dtvdm_ops.delsys[3] = SYS_ISDBT;
			aml_dtvdm_ops.delsys[4] = SYS_DVBS;
			aml_dtvdm_ops.delsys[5] = SYS_DVBT2;
			aml_dtvdm_ops.delsys[6] = SYS_DVBT;
			aml_dtvdm_ops.delsys[7] = SYS_DVBC_ANNEX_B;
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
			aml_dtvdm_ops.delsys[8] = SYS_ANALOG;
#endif
			strcpy(aml_dtvdm_ops.info.name,
					"amlogic DVB-C/T/T2/S/S2/ATSC/ISDBT dtv demod t5d");
			break;

		case DTVDEMOD_HW_S4:
			aml_dtvdm_ops.delsys[0] = SYS_DVBC_ANNEX_A;
			aml_dtvdm_ops.delsys[1] = SYS_DVBC_ANNEX_B;
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
			aml_dtvdm_ops.delsys[2] = SYS_ANALOG;
#endif
			strcpy(aml_dtvdm_ops.info.name, "amlogic DVB-C dtv demod s4");
			break;

		case DTVDEMOD_HW_T3:
			/* max delsys is 8, index: 0~7 */
			aml_dtvdm_ops.delsys[0] = SYS_DVBC_ANNEX_A;
			aml_dtvdm_ops.delsys[1] = SYS_ATSC;
			aml_dtvdm_ops.delsys[2] = SYS_DVBS2;
			aml_dtvdm_ops.delsys[3] = SYS_ISDBT;
			aml_dtvdm_ops.delsys[4] = SYS_DVBS;
			aml_dtvdm_ops.delsys[5] = SYS_DVBT2;
			aml_dtvdm_ops.delsys[6] = SYS_DVBT;
			aml_dtvdm_ops.delsys[7] = SYS_DVBC_ANNEX_B;
			aml_dtvdm_ops.delsys[8] = SYS_DTMB;
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
			aml_dtvdm_ops.delsys[9] = SYS_ANALOG;
#endif
			strcpy(aml_dtvdm_ops.info.name,
					"Aml DVB-C/T/T2/S/S2/ATSC/ISDBT/DTMB ddemod t3");
			break;
		case DTVDEMOD_HW_S4D:
			aml_dtvdm_ops.delsys[0] = SYS_DVBC_ANNEX_A;
			aml_dtvdm_ops.delsys[1] = SYS_DVBC_ANNEX_B;
			aml_dtvdm_ops.delsys[2] = SYS_DVBS;
			aml_dtvdm_ops.delsys[3] = SYS_DVBS2;
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
			aml_dtvdm_ops.delsys[4] = SYS_ANALOG;
#endif
			strcpy(aml_dtvdm_ops.info.name, "amlogic DVB-C/DVB-S dtv demod s4d");
			break;
		case DTVDEMOD_HW_S1A:
			aml_dtvdm_ops.delsys[0] = SYS_DVBC_ANNEX_A;
			aml_dtvdm_ops.delsys[1] = SYS_DVBC_ANNEX_B;
			aml_dtvdm_ops.delsys[2] = SYS_DVBS;
			aml_dtvdm_ops.delsys[3] = SYS_DVBS2;
			aml_dtvdm_ops.delsys[4] = SYS_ANALOG;
			strcpy(aml_dtvdm_ops.info.name, "amlogic DVB-C/DVB-S dtv demod s1a");
			break;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		case DTVDEMOD_HW_T5W:
			/* max delsys is 8, index: 0~7 */
			aml_dtvdm_ops.delsys[0] = SYS_DVBC_ANNEX_A;
			aml_dtvdm_ops.delsys[1] = SYS_ATSC;
			aml_dtvdm_ops.delsys[2] = SYS_DVBS2;
			aml_dtvdm_ops.delsys[3] = SYS_ISDBT;
			aml_dtvdm_ops.delsys[4] = SYS_DVBS;
			aml_dtvdm_ops.delsys[5] = SYS_DVBT2;
			aml_dtvdm_ops.delsys[6] = SYS_DVBT;
			aml_dtvdm_ops.delsys[7] = SYS_DVBC_ANNEX_B;
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
			aml_dtvdm_ops.delsys[8] = SYS_ANALOG;
#endif
			strcpy(aml_dtvdm_ops.info.name,
					"Aml DVB-C/T/T2/S/S2/ATSC/ISDBT ddemod t5w");
			break;

		case DTVDEMOD_HW_T5M:
			/* max delsys is 8, index: 0~7 */
			aml_dtvdm_ops.delsys[0] = SYS_DVBC_ANNEX_A;
			aml_dtvdm_ops.delsys[1] = SYS_ATSC;
			aml_dtvdm_ops.delsys[2] = SYS_DVBS2;
			aml_dtvdm_ops.delsys[3] = SYS_ISDBT;
			aml_dtvdm_ops.delsys[4] = SYS_DVBS;
			aml_dtvdm_ops.delsys[5] = SYS_DVBT2;
			aml_dtvdm_ops.delsys[6] = SYS_DVBT;
			aml_dtvdm_ops.delsys[7] = SYS_DVBC_ANNEX_B;
			aml_dtvdm_ops.delsys[8] = SYS_DTMB;
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
			aml_dtvdm_ops.delsys[9] = SYS_ANALOG;
#endif
			strcpy(aml_dtvdm_ops.info.name,
					"Aml DVB-C/T/T2/S/S2/ATSC/ISDBT/DTMB ddemod t5m");
			break;

		case DTVDEMOD_HW_T3X:
			/* max delsys is 8, index: 0~7 */
			aml_dtvdm_ops.delsys[0] = SYS_DVBC_ANNEX_A;
			aml_dtvdm_ops.delsys[1] = SYS_ATSC;
			aml_dtvdm_ops.delsys[2] = SYS_DVBS2;
			aml_dtvdm_ops.delsys[3] = SYS_ISDBT;
			aml_dtvdm_ops.delsys[4] = SYS_DVBS;
			aml_dtvdm_ops.delsys[5] = SYS_DVBT2;
			aml_dtvdm_ops.delsys[6] = SYS_DVBT;
			aml_dtvdm_ops.delsys[7] = SYS_DVBC_ANNEX_B;
			aml_dtvdm_ops.delsys[8] = SYS_DTMB;
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
			aml_dtvdm_ops.delsys[9] = SYS_ANALOG;
#endif
			strcpy(aml_dtvdm_ops.info.name,
					"Aml DVB-C/T/T2/S/S2/ATSC/ISDBT/DTMB ddemod t3x");
			break;

		case DTVDEMOD_HW_TXHD2:
			aml_dtvdm_ops.delsys[0] = SYS_DTMB;
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
			aml_dtvdm_ops.delsys[1] = SYS_ANALOG;
#endif
			break;
#endif //end of CONFIG_AMLOGIC_ZAPPER_CUT

		default:
			ic_is_supportted = 0;
			PR_ERR("%s: error unsupported ic=%d\n", __func__, ic_version);
			kfree(demod);
			mutex_unlock(&amldtvdemod_device_mutex);

			return NULL;
		}
		break;
	}

	memcpy(&demod->frontend.ops, &aml_dtvdm_ops, sizeof(struct dvb_frontend_ops));

	/*diseqc attach*/
#ifdef AML_DEMOD_SUPPORT_DVBS
	if (!IS_ERR_OR_NULL(devp->diseqc.name))
		aml_diseqc_attach(devp->dev, &demod->frontend);
#endif
	list_add_tail(&demod->list, &devp->demod_list);

	devp->index++;

	PR_INFO("%s [id = %d, total attach: %d] OK.\n",
			__func__, demod->id, devp->index);

	mutex_unlock(&amldtvdemod_device_mutex);

	return &demod->frontend;
}
EXPORT_SYMBOL_GPL(aml_dtvdm_attach);

#ifdef AML_DTVDEMOD_EXP_ATTACH
static struct aml_exp_func aml_exp_ops = {
	.leave_mode = leave_mode,
};

struct aml_exp_func *aml_dtvdm_exp_attach(struct aml_exp_func *exp)
{
	if (exp) {
		memcpy(exp, &aml_exp_ops, sizeof(struct aml_exp_func));
	} else {
		PR_ERR("%s:fail!\n", __func__);
		return NULL;

	}
	return exp;
}
EXPORT_SYMBOL_GPL(aml_dtvdm_exp_attach);

void aml_exp_attach(struct aml_exp_func *afe)
{

}
EXPORT_SYMBOL_GPL(aml_exp_attach);
#endif /* AML_DTVDEMOD_EXP_ATTACH */

//#ifndef MODULE
//fs_initcall(aml_dtvdemod_init);
//module_exit(aml_dtvdemod_exit);

//MODULE_VERSION(AMLDTVDEMOD_VER);
//MODULE_DESCRIPTION("gxtv_demod DVB-T/DVB-C/DTMB Demodulator driver");
//MODULE_AUTHOR("RSJ");
//MODULE_LICENSE("GPL");
//#endif
