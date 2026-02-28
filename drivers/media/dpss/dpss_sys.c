// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include "sys_def.h"		//ary add for sim
//#include "define.h"
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/amlogic/tee.h>

#ifdef RUN_ON_ARM

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/uaccess.h>
#include <linux/of_fdt.h>
#include <linux/cma.h>
#include <linux/dma-map-ops.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/compat.h>
#include <linux/of_device.h>
#include <linux/clk-provider.h>
#include <linux/interrupt.h>

#include <linux/freezer.h>	//for task
#include <linux/kfifo.h>
#include <uapi/linux/sched/types.h>
#include <uapi/amlogic/frc.h>

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>

#include <linux/amlogic/media/vpu/vpu.h>
/*dma_get_cma_size_int_byte*/
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/di/dpss_interface.h>
#endif				/* RUN_ON_ARM */
#include <linux/amlogic/media/dpss/frc_common_x.h>
#include <linux/amlogic/media/dpss/dpss_frc.h>

#include "dpss_base.h"
#include "dpss_s.h"
#include "dpss_sys.h"
#include "dpss_func.h"
#include "dpss_s_frc.h"
#include "drv/d_pq.h"
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
#endif

/*2024-05-10*/
unsigned int dpss_uint_up_bits(unsigned int val_ori, unsigned int value,
			       unsigned int start, unsigned int len)
{
	unsigned int mask = 0;
	unsigned int val = 0;
	unsigned int tmp;

	if ((len + start) > 32) {
		DBG_ERR("%s:0x%x,0x%x,%d,%d\n", "up_bits",
			val_ori, val, start, len);
		return val;
	}

	mask = ((1 << (len)) - 1) << (start);
	val = (value) << (start);
	tmp = val_ori & (~mask);
	tmp |= val & mask;
	return tmp;
}

/********************************************/
//#if 1 register access:
bool tst_dbg_reg(void)
{
	if (dpss_wr_sel & C_BIT1)
		return true;
	return false;
}

void reg_dpss_wr(unsigned int addr, unsigned int val)
{
	aml_write_dpss(addr, val);
}

static unsigned int reg_dpss_read(unsigned int addr)
{
	unsigned int d;

	d = (unsigned int)aml_read_dpss(addr);

	return d;
}

unsigned int reg_dpss_wr_bits(unsigned int addr, unsigned int val,
			      unsigned int start, unsigned int len)
{
	unsigned int val_ori, val_new;

	val_ori = reg_dpss_read(addr);

	val_new = dpss_uint_up_bits(val_ori, val, start, len);
	reg_dpss_wr(addr, val_new);

	return val_new;
}

unsigned int reg_dpss_rd_bits(unsigned int addr, unsigned int start,
			      unsigned int len)
{
	unsigned int val_ori, val_new;

	val_ori = reg_dpss_read(addr);
	val_new = get_reg_bits(val_ori, start, len);
	return val_new;
}

const struct reg_acc t6w_dpss_reg_acc = {
	.wr = reg_dpss_wr,
	.rd = reg_dpss_read,
	.bwr = reg_dpss_wr_bits,
	.brd = reg_dpss_rd_bits,
};

void reg_vc_wr(unsigned int addr, unsigned int val)
{
	aml_write_vcbus(addr, val);
}

static unsigned int reg_vc_read(unsigned int addr)
{
	unsigned int d;

	d = (unsigned int)aml_read_vcbus(addr);

	return d;
}

unsigned int reg_vc_wr_bits(unsigned int addr, unsigned int val,
			    unsigned int start, unsigned int len)
{
	unsigned int val_ori, val_new;

	val_ori = reg_vc_read(addr);

	val_new = dpss_uint_up_bits(val_ori, val, start, len);
	reg_vc_wr(addr, val_new);

	return val_new;
}

unsigned int reg_vc_rd_bits(unsigned int addr, unsigned int start,
			    unsigned int len)
{
	unsigned int val_ori, val_new;

	val_ori = reg_vc_read(addr);
	val_new = get_reg_bits(val_ori, start, len);
	return val_new;
}

const struct reg_acc t6w_vcbus_reg_acc = {
	.wr = reg_vc_wr,
	.rd = reg_vc_read,
	.bwr = reg_vc_wr_bits,
	.brd = reg_vc_rd_bits,
};

//#endif //

#ifdef RUN_ON_ARM
void wr(unsigned int addr, unsigned int val)
{
	const struct reg_acc *op = &t6w_dpss_reg_acc;

	if (dpss_wr_sel & C_BIT16) {
		dpss_pre_tab_add_w_dpss(addr, val);
		return;
	}
	op->wr(addr, val);
	if (tst_dbg_reg())
		base_pr("d_wr:0x%x,0x%08x,0,32;\n", addr, val);
}

unsigned int rd(unsigned int addr)
{
	const struct reg_acc *op = &t6w_dpss_reg_acc;
	unsigned int val;

	val = op->rd(addr);
	if (tst_dbg_reg())
		base_pr("d_rd:0x%x,0x%08x,0,32;\n", addr, val);
	return val;
}

unsigned int w_reg_bit(unsigned int addr, unsigned int val,
		       unsigned int start, unsigned int len)
{
	const struct reg_acc *op = &t6w_dpss_reg_acc;

	if (dpss_wr_sel & C_BIT16) {
		dpss_pre_tab_add_wb_dpss(addr, val, start, len);
		return 1;
	}
	op->bwr(addr, val, start, len);

	if (tst_dbg_reg())
		base_pr("d_wb:0x%x,0x%x,%d,%d\n", addr, val, start, len);
	return 1;
}

void wr_vc(unsigned int addr, unsigned int val)
{
	const struct reg_acc *op = &t6w_vcbus_reg_acc;

	if (dpss_wr_sel & C_BIT16) {
		dpss_pre_tab_add_w_vc(addr, val);
		return;
	}

	op->wr(addr, val);
	if (tst_dbg_reg())
		base_pr("vc_wr:0x%x,0x%08x,0,32;\n", addr, val);
}

unsigned int rd_vc(unsigned int addr)
{
	const struct reg_acc *op = &t6w_vcbus_reg_acc;
	unsigned int val;

	val = op->rd(addr);
	if (tst_dbg_reg())
		base_pr("vc_rd:0x%x,0x%08x,0,32;\n", addr, val);
	return val;
}

unsigned int w_reg_bit_vc(unsigned int addr, unsigned int val,
			  unsigned int start, unsigned int len)
{
	const struct reg_acc *op = &t6w_vcbus_reg_acc;

	if (dpss_wr_sel & C_BIT16) {
		dpss_pre_tab_add_wb_vc(addr, val, start, len);
		return 1;
	}
	op->bwr(addr, val, start, len);

	if (tst_dbg_reg())
		base_pr("vc_wb:0x%x,0x%x,%d,%d\n", addr, val, start, len);
	return 1;
}

#endif				/* RUN_ON_ARM */

//#if 1 //simulus only tmp
void send_event_to_sv(unsigned int event, unsigned int val)
{
	//DPSS_DONE_EVENT, (dpss_disp1_frm_cnt<<8) | dpss_disp0_frm_cnt
}

void stimulus_wait_event_done(unsigned int event)
{
	//DPSS_DONE_EVENT
}

void set_int_type(unsigned int irq_vec, unsigned int type)
{
	// GIC_INT_VEC(SPI_VDIN0_VSYNC) , INT_TYPE_IRQ
}

void set_int_enable(unsigned int irq_vec)
{
	//GIC_INT_VEC(SPI_VDIN0_VSYNC)
}

void stimulus_finish_pass(void)
{
}

void stimulus_finish_fail(unsigned int ty)
{
}

//#endif
/* ary */
static unsigned int dpss_pause;
module_param_named(dpss_pause, dpss_pause, uint, 0664);

bool dpss_dbg_is_run(void)
{
	bool ret = false;

	if (dpss_pause == DPSS_RUN_FLAG_RUN || dpss_pause == DPSS_RUN_FLAG_STEP) {
		if (dpss_pause == DPSS_RUN_FLAG_STEP)
			dpss_pause = DPSS_RUN_FLAG_STEP_DONE;
		ret = true;
	}

	return ret;
}

void dpss_info_reg(char *name)
{
	struct dpss_dev_s *devp = NULL;
	unsigned char pos;
	//unsigned int info;

	devp = dpss_get_devp();
	if (!devp)
		return;
	pos = devp->info_pos;
	devp->info_pos++;
	devp->info[pos / 8] |= 1 << (pos % 8);
	DBG_WARN("reg_info:%s:%d\n", name, pos);
}

/********************************************/
#ifdef RUN_ON_ARM		//task

static int _tsk_m_is_exiting(struct task_dpss_m_s *tsk)
{
	if (tsk->exit)
		return 1;

/*	if (afepriv->dvbdev->writers == 1)
 *		if (time_after_eq(jiffies, fepriv->release_jiffies +
 *				  dvb_shutdown_timeout * HZ))
 *			return 1;
 */
	return 0;
}

static int _tsk_m_should_wakeup(struct task_dpss_m_s *tsk)
{
	if (tsk->wakeup) {
		tsk->wakeup = 0;
		/*dbg only dbg_tsk("wkg[%d]\n", di_dbg_task_flg); */
		return 1;
	}
	return _tsk_m_is_exiting(tsk);
}

static void _tsk_m_wakeup(struct task_dpss_m_s *tsk)
{
	tsk->wakeup = 1;
	wake_up_interruptible(&tsk->wait_queue);
	/*dbg_tsk("wks[%d]\n", di_dbg_task_flg); */
}

static int tsk_m_thread(void *data)
{
	struct task_dpss_m_s *tsk = data;
	bool semheld = false;

	tsk->delay = HZ;
	tsk->status = 0;
	tsk->wakeup = 0;

	set_freezable();
	while (1) {
		up(&tsk->sem);	/* is locked when we enter the thread... */
restart:
		wait_event_interruptible_timeout(tsk->wait_queue,
						 _tsk_m_should_wakeup(tsk) ||
						 kthread_should_stop() ||
						 freezing(current), tsk->delay);

		if (kthread_should_stop() || _tsk_m_is_exiting(tsk)) {
			/* got signal or quitting */
			if (!down_interruptible(&tsk->sem))
				semheld = true;
			tsk->exit = 1;
			break;
		}

		if (try_to_freeze())
			goto restart;

		if (tsk->shut_down_flag)
			break;
		if (down_interruptible(&tsk->sem))
			break;

		dpss_tsk_m_polling();
	}

	complete(&tsk->task_done);
	tsk->thread = NULL;
	if (kthread_should_stop())
		tsk->exit = 1;
	else
		tsk->exit = 0;
	/*mb(); */

	if (semheld)
		up(&tsk->sem);

	_tsk_m_wakeup(tsk);	/*? */
	return 0;
}

static void _tsk_m_stop(void)/*struct task_dpss_m_s *tsk */
{
	struct task_dpss_m_s *tsk = dpss_get_m_task();
	int i = 0;

	/*not use cmd */
	dbg_s0("%s %d\n", __func__, i);
	/*--------------------*/
	/*cmd buf */
	if (tsk->flg_cmd) {
		kfifo_free(&tsk->fifo_cmd);
		tsk->flg_cmd = 0;
	}
	/*tsk->lock_cmd = SPIN_LOCK_UNLOCKED; */
	spin_lock_init(&tsk->lock_cmd);

	/*cmd2 buf */
	for (i = 0; i < DPSS_CHANNEL_NUB; i++) {
		if (tsk->flg_cmd2[i]) {
			kfifo_free(&tsk->fifo_cmd2[i]);
			tsk->flg_cmd2[i] = 0;
		}

		spin_lock_init(&tsk->lock_cmd2[i]);
	}
	tsk->err_cmd_cnt = 0;
	/*--------------------*/

	tsk->exit = 1;
	/*mb(); */

	if (!tsk->thread)
		return;

	kthread_stop(tsk->thread);

	sema_init(&tsk->sem, 1);
	tsk->status = 0;

	/* paranoia check in case a signal arrived */
	if (tsk->thread)
		DBG_ERR("warning: thread %p won't exit\n", tsk->thread);
}

static int _tsk_m_start(void)
{
	int ret;
	int flg_err;
	struct task_dpss_m_s *tsk = dpss_get_m_task();
	int i;
	struct task_struct *fe_thread;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1 };

	flg_err = 0;
	dbg_s0("%s flg_err:%d\n", __func__, flg_err);
	/*not use cmd */
	/*--------------------*/
	/*cmd buf */
	/*tsk->lock_cmd = SPIN_LOCK_UNLOCKED; */
	spin_lock_init(&tsk->lock_cmd);
	tsk->err_cmd_cnt = 0;
	ret = kfifo_alloc(&tsk->fifo_cmd,
			  /*sizeof(unsigned int) * MAX_KFIFO_L_CMD_NUB */ 8,
			  GFP_KERNEL);
	if (ret < 0) {
		tsk->flg_cmd = false;
		DBG_ERR("%s:can't get kfifo\n", __func__);
		return -1;
	}
	init_completion(&tsk->task_done);
	tsk->flg_cmd = true;

	for (i = 0; i < DPSS_CHANNEL_NUB; i++) {
		spin_lock_init(&tsk->lock_cmd2[i]);
		ret = kfifo_alloc(&tsk->fifo_cmd2[i],
				  /*sizeof(unsigned int) * MAX_KFIFO_L_CMD_NUB */
				  8,
				  GFP_KERNEL);
		if (ret < 0) {
			tsk->flg_cmd2[i] = false;
			DBG_ERR("%s:can't get kfifo2,ch[%d]\n", __func__, i);
			flg_err++;
			break;
		}

		tsk->flg_cmd2[i] = true;
	}

	if (flg_err) {
		if (tsk->flg_cmd) {
			kfifo_free(&tsk->fifo_cmd);
			tsk->flg_cmd = false;
		}
		for (i = 0; i < DPSS_CHANNEL_NUB; i++) {
			if (tsk->flg_cmd2[i]) {
				kfifo_free(&tsk->fifo_cmd2[i]);
				tsk->flg_cmd2[i] = false;
			}
		}
		return -1;
	}
	/*--------------------*/
	sema_init(&tsk->sem, 1);
	init_waitqueue_head(&tsk->wait_queue);

	if (tsk->thread) {
		if (!tsk->exit)
			return 0;

		_tsk_m_stop();
	}

	if (signal_pending(current)) {
		if (tsk->flg_cmd) {
			kfifo_free(&tsk->fifo_cmd);
			tsk->flg_cmd = 0;
		}
		for (i = 0; i < DPSS_CHANNEL_NUB; i++) {
			if (tsk->flg_cmd2[i]) {
				kfifo_free(&tsk->fifo_cmd2[i]);
				tsk->flg_cmd2[i] = 0;
			}
		}
		return -EINTR;
	}
	if (down_interruptible(&tsk->sem)) {
		if (tsk->flg_cmd) {
			kfifo_free(&tsk->fifo_cmd);
			tsk->flg_cmd = 0;
		}
		for (i = 0; i < DPSS_CHANNEL_NUB; i++) {
			if (tsk->flg_cmd2[i]) {
				kfifo_free(&tsk->fifo_cmd2[i]);
				tsk->flg_cmd2[i] = 0;
			}
		}
		return -EINTR;
	}

	tsk->status = 0;
	tsk->exit = 0;
	tsk->thread = NULL;
	/*mb(); */

	fe_thread = kthread_run(tsk_m_thread, tsk, "aml-dpss-m");
	if (IS_ERR(fe_thread)) {
		ret = PTR_ERR(fe_thread);
		DBG_ERR(" failed to start kthread (%d)\n", ret);
		up(&tsk->sem);
		tsk->flg_init = 0;
		return ret;
	}

	sched_setscheduler_nocheck(fe_thread, SCHED_FIFO, &param);
	tsk->flg_init = 1;
	tsk->thread = fe_thread;
	return 0;
}

void dpss_tsk_m_wake(unsigned int id)
{
	struct task_dpss_m_s *tsk = dpss_get_m_task();

	_tsk_m_wakeup(tsk);
	dbg_s2("trig:r:%u\n", id);
}

void dpss_tsk_m_delay(unsigned int val)
{
	struct task_dpss_m_s *tsk = dpss_get_m_task();

	tsk->delay = HZ / val;
}

/* add mem serverd task */
static int _tsk_s_is_exiting(struct dpss_mtask *tsk)
{
	if (tsk->exit)
		return 1;

/*	if (afepriv->dvbdev->writers == 1)
 *		if (time_after_eq(jiffies, fepriv->release_jiffies +
 *				  dvb_shutdown_timeout * HZ))
 *			return 1;
 */
	return 0;
}

static int _tst_s_should_wakeup(struct dpss_mtask *tsk)
{
	if (tsk->wakeup) {
		tsk->wakeup = 0;
		/*dbg only dbg_tsk("wkg[%d]\n", di_dbg_task_flg); */
		return 1;
	}
	return _tsk_s_is_exiting(tsk);
}

static void _tsk_s_wakeup(struct dpss_mtask *tsk)
{
	tsk->wakeup = 1;
	wake_up_interruptible(&tsk->wait_queue);
	/*dbg_tsk("wks[%d]\n", di_dbg_task_flg); */
}

void dpss_tsk_s_wake(void)
{
	struct dpss_mtask *tsk = dpss_get_s_task();

	_tsk_s_wakeup(tsk);
}

static int tsk_s_thread(void *data)
{
	struct dpss_mtask *tsk = data;
	bool semheld = false;
//      int i;
//      struct di_ch_s *pch;

	tsk->delay = HZ;
	tsk->status = 0;
	tsk->wakeup = 0;

	set_freezable();
	while (1) {
		up(&tsk->sem);	/* is locked when we enter the thread... */
mrestart:
		wait_event_interruptible_timeout(tsk->wait_queue,
						 _tst_s_should_wakeup(tsk) ||
						 kthread_should_stop() ||
						 freezing(current), tsk->delay);

		if (kthread_should_stop() || _tsk_s_is_exiting(tsk)) {
			/* got signal or quitting */
			if (!down_interruptible(&tsk->sem))
				semheld = true;
			tsk->exit = 1;
			break;
		}

		if (try_to_freeze())
			goto mrestart;

		if (down_interruptible(&tsk->sem))
			break;

		dpss_tsk_s_polling();
	}

	tsk->thread = NULL;
	if (kthread_should_stop())
		tsk->exit = 1;
	else
		tsk->exit = 0;
	/*mb(); */

	if (semheld)
		up(&tsk->sem);

	_tsk_s_wakeup(tsk);	/*? */
	return 0;
}

static void _tsk_s_rs_alloc(struct dpss_mtask *tsk)
{
	int i;
	struct dpss_fcmd_s *fcmd;
	int ret;

	for (i = 0; i < DPSS_CHANNEL_NUB; i++) {
		/*ini */
		fcmd = &tsk->fcmd[i];
		fcmd->flg_lock = 0;

		if (fcmd->flg_lock & DPSS_QUE_LOCK_RD)
			spin_lock_init(&fcmd->lock_r);
		if (fcmd->flg_lock & DPSS_QUE_LOCK_WR)
			spin_lock_init(&fcmd->lock_w);

		ret = kfifo_alloc(&fcmd->fifo,
				  /*sizeof(struct mtsk_cmd_s) * MAX_KFIFO_L_CMD_NUB*/
				  8, GFP_KERNEL);
		if (ret < 0) {
			fcmd->flg = false;
			tsk->err_res++;
			DBG_ERR("%s:can't get kfifo2,ch[%d]\n", __func__, i);
			break;
		}
		init_completion(&fcmd->alloc_done);
		fcmd->flg = true;
	}
}

static void _tsk_s_rs_release(struct dpss_mtask *tsk)
{
	int i;
	struct dpss_fcmd_s *fcmd;

	for (i = 0; i < DPSS_CHANNEL_NUB; i++) {
		fcmd = &tsk->fcmd[i];

		if (fcmd->flg) {
			kfifo_free(&fcmd->fifo);
			fcmd->flg = false;
		}
		if (fcmd->flg_lock & DPSS_QUE_LOCK_RD)
			spin_lock_init(&fcmd->lock_r);
		if (fcmd->flg_lock & DPSS_QUE_LOCK_WR)
			spin_lock_init(&fcmd->lock_w);
	}
	tsk->err_res = 0;
}

static void _tsk_s_stop(void)
{
	struct dpss_mtask *tsk = dpss_get_s_task();
	int i = 0;

	dbg_s1("%s %d", __func__, i);
	/*--------------------*/
	_tsk_s_rs_release(tsk);
	/*--------------------*/

	tsk->exit = 1;
	/*mb(); */

	if (!tsk->thread)
		return;

	kthread_stop(tsk->thread);

	sema_init(&tsk->sem, 1);
	tsk->status = 0;

	/* paranoia check in case a signal arrived */
	if (tsk->thread)
		DBG_ERR("warning: thread %p won't exit\n", tsk->thread);
	dbg_s1("%s:finish\n", __func__);
}

static int _tsk_s_start(void)
{
	int ret;
	int flg_err;
	struct dpss_mtask *tsk = dpss_get_s_task();
	struct task_struct *fe_thread;
	//no need high priority struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	flg_err = 0;
	dbg_s0("%s flg_err %d\n", __func__, flg_err);
	/*not use cmd */
	/*--------------------*/
	_tsk_s_rs_alloc(tsk);

	/*--------------------*/
	sema_init(&tsk->sem, 1);
	init_waitqueue_head(&tsk->wait_queue);

	if (tsk->thread) {
		if (!tsk->exit)
			return 0;

		_tsk_s_stop();
	}

	if (signal_pending(current)) {
		_tsk_s_rs_release(tsk);

		return -EINTR;
	}
	if (down_interruptible(&tsk->sem)) {
		_tsk_s_rs_release(tsk);

		return -EINTR;
	}

	tsk->status = 0;
	tsk->exit = 0;
	tsk->thread = NULL;
	/*mb(); */

	fe_thread = kthread_run(tsk_s_thread, tsk, "aml-dpss-sub");
	if (IS_ERR(fe_thread)) {
		ret = PTR_ERR(fe_thread);
		DBG_ERR(" failed to start kthread (%d)\n", ret);
		up(&tsk->sem);
		tsk->flg_init = 0;
		return ret;
	}

	//no need high priority sched_setscheduler_nocheck(fe_thread, SCHED_FIFO, &param);
	tsk->flg_init = 1;
	tsk->thread = fe_thread;
	return 0;
}

void dpss_mtask_wakeup(void)//dbg_mtask
{
	struct dpss_mtask *tsk = dpss_get_s_task();

	tsk->status = 1;
	_tsk_s_wakeup(tsk);
}
#else
//run on pc:
static int _tsk_m_start(void)//run on pc
{
	return 0;
}

static void _tsk_s_stop(void)//run on pc
{
}

static int _tsk_s_start(void)//run on pc
{
	return 0;
}

void dpss_tsk_m_delay(unsigned int val)
{
}

void dpss_tsk_m_wake(unsigned int id)
{
}

void dpss_tsk_s_wake(void)
{
}

#endif				/* task function */
//task:
static void tsk_m_stop(void)
{
}

static int tsk_m_start(void)
{
	return _tsk_m_start();
}

static void tsk_s_stop(void)
{
	_tsk_s_stop();
}

static int tsk_s_start(void)
{
	int ret;

	ret = _tsk_s_start();
	return ret;
}

/********************************************/
#ifdef RUN_ON_ARM
static void _parse_cmd_params(char *buf_orig, char **parm)
{
	char *ps, *token;
	char delim1[3] = " ";
	char delim2[2] = "\n";
	unsigned int n = 0;

	strcat(delim1, delim2);
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}

static ssize_t config_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	int pos = 0;

	return pos;
}

static ssize_t dump_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	int pos = 0;

	return pos;
}

static ssize_t config_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
//      int rc = 0;
	char *parm[2] = { NULL }, *buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	_parse_cmd_params(buf_orig, (char **)(&parm));

	if (strncmp(buf, "disable", 7) == 0)
		DBG_INF("class: cfg:disable\n");
	else if (strncmp(buf, "dis2", 4) == 0)
		DBG_INF("class: cfg:dis2\n");

	kfree(buf_orig);
	return count;
}

static ssize_t reg_store(struct device *dev,
			 struct device_attribute *attr,
			 const char *buf, size_t count)
{
//      int rc = 0;
	char *parm[4] = { NULL }, *buf_orig;
	unsigned int addr;
	unsigned int val, rval;
	int i;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	_parse_cmd_params(buf_orig, (char **)(&parm));

	if (strncmp(parm[0], "wr", 2) == 0) {
		//wr
		if (kstrtouint(parm[1], 0, &addr)) {
			DBG_ERR("parm 1: not number\n");
			kfree(buf_orig);
			return 0;
		}
		if (kstrtouint(parm[2], 0, &val)) {
			DBG_ERR("parm 1: not number\n");
			kfree(buf_orig);
			return 0;
		}
		pr_info("wr 0x%x 0x%x\n", addr, val);
		aml_write_dpss(addr, val);

	} else if (strncmp(parm[0], "rd", 2) == 0) {
		//rd
		if (kstrtouint(parm[1], 0, &addr)) {
			DBG_ERR("parm 1: not number\n");
			kfree(buf_orig);
			return 0;
		}
		val = aml_read_dpss(addr);
		pr_info("rd 0x%x 0x%x\n", addr, val);
	} else if (strncmp(parm[0], "rs", 2) == 0) {
		if (kstrtouint(parm[1], 0, &addr)) {
			DBG_ERR("parm 1: not number\n");
			kfree(buf_orig);
			return 0;
		}
		if (kstrtouint(parm[2], 0, &val)) {
			DBG_ERR("parm 1: not number\n");
			kfree(buf_orig);
			return 0;
		}
		for (i = 0; i < val; i++) {
			rval = aml_read_dpss(addr + i);
			pr_info("rd 0x%x 0x%x\n", addr + i, rval);
		}
	}
	kfree(buf_orig);
	return count;
}

#ifndef DBG_TEST_SAVE_VFM
static ssize_t dump_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
//      int rc = 0;
	char *parm[2] = { NULL }, *buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	_parse_cmd_params(buf_orig, (char **)(&parm));

	DBG_INF("nothing\n");

	kfree(buf_orig);
	return count;
}

#endif

//class
static DEVICE_ATTR(config, 0644, config_show, config_store);
static DEVICE_ATTR(reg, 0200, NULL, reg_store);
static DEVICE_ATTR(dump, 0644, dump_show, dump_store);

#endif				//RUN_ON_ARM
/********************************************/
unsigned int dpss_dbg;
module_param_named(dpss_dbg, dpss_dbg, uint, 0664);

static struct dpss_dev_s *dpss_pdev;	//di_pdev

struct dpss_dev_s *dpss_get_devp(void)
{
	return dpss_pdev;
}

//level : 0, 1, 2, 3
// 0: disable all debug info;
// 1: enable dbg_level0;
// 2: enable dbg_level 0, 1;...
void dpss_dbg_level_set(unsigned int level)
{
	if (level > 3) {
		DBG_WARN("%s:%d overflow, do nothing\n", __func__, level);
		return;
	}
	dpss_dbg &= (~0x3);
	dpss_dbg |= level & 0x03;
}

unsigned int dpss_dbg_level_get(void)
{
	return (dpss_dbg & 0x03);
}

/********************************************/
#ifdef C_TST_ON_T6D
static void _vpu_dev_register(struct dpss_dev_s *vdevp)
{
	vdevp->dim_vpu_clk_gate_dev = vpu_dev_register(VPU_VPU_CLKB,
						       DEVICE_NAME);
	vdevp->dim_vpu_pd_dec = vpu_dev_register(VPU_AFBC_DEC, DEVICE_NAME);
	vdevp->dim_vpu_pd_dec1 = vpu_dev_register(VPU_AFBC_DEC1, DEVICE_NAME);
	vdevp->dim_vpu_pd_vd1 = vpu_dev_register(VPU_VIU_VD1, DEVICE_NAME);
	vdevp->dim_vpu_pd_post = vpu_dev_register(VPU_DI_POST, DEVICE_NAME);
}
#endif				/* C_TST_ON_T6D */

static void dpss_prob_other(struct dpss_dev_s *vdevp)
{
#ifdef C_TST_ON_T6D
	_vpu_dev_register(vdevp);
#endif				/* C_TST_ON_T6D */
	//to-do dim_debugfs_init();
}

#ifdef RUN_ON_ARM
static int dpss_open(struct inode *node, struct file *file)
{
	struct dpss_dev_s *devp;

/* Get the per-device structure that contains this cdev */
	devp = container_of(node->i_cdev, struct dpss_dev_s, cdev);
	file->private_data = devp;

	return 0;
}

static int dpss_release(struct inode *node, struct file *file)
{
/* di_dev_t *di_in_devp = file->private_data; */

/* Reset file pointer */

/* Release some other fields */
	file->private_data = NULL;
	return 0;
}

static long dpss_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	/* to-do */
	int ret = 0;
	struct dpss_dev_s *devp;
	struct dpss_frc_fw_data_s *pfw_data;
	struct frc_chip_st *pchip_st;
	void __user *argp = (void __user *)arg;
	u32 data;
	struct memc_gmv_s memc_gmv;
	enum frc_fpp_state_e fpp_state;

	memc_gmv.gmv_x = 0;
	memc_gmv.gmv_y = 0;

	devp = file->private_data;
	if (!devp)
		return -EFAULT;
	if (devp->flags & DI_SUSPEND_FLAG) {
		dbg_s0("stop io when suspend\n");
		return -EFAULT;
	}
	pchip_st = devp->pchip_st;
	if (!pchip_st)
		return -EFAULT;

	if (pchip_st->dbg_st.ctrl_dbg.disable_io_ctrl == 1) {
		pr_frc(0, "disable dpss_frc ioctrl\n");
		return ret;
	}
	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data)
		return -EFAULT;

	switch (cmd) {
	case DPSS_FRC_IOC_SET_DEBLUR_LEVEL:
		if (copy_from_user(&data, argp, sizeof(u32))) {
			ret = -EFAULT;
			break;
		}
		dpss_frc_set_deblur(data);
		break;

	case DPSS_FRC_IOC_SET_MEMC_ON_OFF:
		if (copy_from_user(&data, argp, sizeof(u32))) {
			ret = -EFAULT;
			break;
		}
		if (pchip_st->dbg_st.ctrl_dbg.ui_frc_state_sel)
			dpss_frc_set_mc_bypass(data);
		else
			dpss_set_mc_link_state(data);
		break;

	case DPSS_FRC_IOC_SET_MEMC_LEVEL:
		if (copy_from_user(&data, argp, sizeof(u32))) {
			ret = -EFAULT;
			break;
		}
		dpss_frc_set_dejudder(data);
		// pr_frc(1, "SET_MEMC_LEVEL:%d\n", data);
		break;

	case DPSS_FRC_IOC_SET_FPP_MEMC_LEVEL:
		if (copy_from_user(&fpp_state, argp,
			sizeof(enum frc_fpp_state_e))) {
			pr_frc(1, "fpp copy from user error!/n");
			ret = -EFAULT;
			break;
		}
		if (fpp_state == FPP_MEMC_OFF)
			dpss_frc_fpp_memc_set_level(0, 0);
		else if (fpp_state == FPP_MEMC_24PFILM)
			dpss_frc_fpp_memc_set_level(10, 2);
		else if (fpp_state == FPP_MEMC_LOW)
			dpss_frc_fpp_memc_set_level(3, 0);
		else if (fpp_state == FPP_MEMC_MID)
			dpss_frc_fpp_memc_set_level(6, 0);
		else if (fpp_state == FPP_MEMC_HIGH)
			dpss_frc_fpp_memc_set_level(10, 0);
		else
			dpss_frc_fpp_memc_set_level((u8)fpp_state, 0);
		pr_frc(1, "set_fpp_memc_level:%d\n", fpp_state);
		break;

	case DPSS_FRC_IOC_SET_MEMC_DMEO_MODE:
		if (copy_from_user(&data, argp, sizeof(u32))) {
			ret = -EFAULT;
			break;
		}
		dpss_frc_demo_win(data, 0);
		break;
	case DPSS_FRC_IOC_GET_MEMC_VERSION:
		data = 0x1111;
		if (copy_to_user(argp, &pfw_data->frc_alg_ver[0], FRC_ALG_VER_SIZE))
			ret = -EFAULT;
		break;
	case DPSS_FRC_IOC_GET_VIDEO_LATENCY:
		data = (u32)dpss_frc_get_video_latency();
		if (copy_to_user(argp, &data, sizeof(u32)))
			ret = -EFAULT;
		break;
	case DPSS_FRC_IOC_SET_MEMC_VENDOR:
		if (copy_from_user(&data, argp, sizeof(u32))) {
			ret = -EFAULT;
			break;
		}
		dpss_frc_tell_alg_vendor(data);
		break;
	case DPSS_FRC_IOC_GET_MEMC_GMV:
		dpss_frc_get_memc_gmv(&memc_gmv);
		if (copy_to_user(argp, &memc_gmv, sizeof(memc_gmv)))
			ret = -EFAULT;
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long dpss_compat_ioctl(struct file *file, unsigned int cmd,
			      unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = dpss_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations dpss_fops = {
	.owner = THIS_MODULE,
	.open = dpss_open,
	.release = dpss_release,
	.unlocked_ioctl = dpss_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = dpss_compat_ioctl,
#endif
};

static const struct dpss_meson_data data_t6d = {
	.name = "t6d",
	.ic_id = IC_ID_T6D,
};

static const struct dpss_meson_data data_t6w = {
	.name = "t6w",
	.ic_id = IC_ID_T6W,
};

static const struct dpss_meson_data data_t6x = {
	.name = "t6x",
	.ic_id = IC_ID_T6X,
};

/* #ifdef CONFIG_USE_OF */
static const struct of_device_id amlogic_dpss_dt_match[] = {
	/*{ .compatible = "amlogic, deinterlace", }, */
	{
		.compatible = "amlogic, dpss-t6d",
		.data = &data_t6d,
	},
	{
		.compatible = "amlogic, dpss-t6w",
		.data = &data_t6w,
	},
	{
		.compatible = "amlogic, dpss-t6x",
		.data = &data_t6x,
	},
	{ }
};
#endif				//RUN_ON_ARM
//-----------------
#ifdef FUNC_EN_BRK
#include "./sub/brk_local.c"
#include "./sub/brk.c"
#include "./sub/brk_p.c"
#include "./sub/brk_r.c"
#endif				/* FUNC_EN_BRK */
//-----------------
#ifdef FUNC_EN_DBG_TREE
#include "./sub/dpss_dbg.c"
#else				/* FUNC_EN_DBG_TREE */
void dpss_dbg_fs_init(void)
{
}

void dpss_dbg_fs_exit(void)
{
}

#endif				/* FUNC_EN_DBG_TREE */

const char *dpss_irq_name_tab[] = {
	"irq_pre_vs",
	"irq_buf_rls1",
	"irq_buf_rls0",
	"irq_inp",
	"irq_dae2",
	"irq_dae1",
	"irq_dae0",
	"irq_dpe2",
	"irq_dpe1",
	"irq_dpe0",
	"irq_rdma",
	"irq_dbg",
};

const char *get_irq_name(unsigned int id)
{
	if (id >= DPSS_IRQ_ID_NUB) {
		DBG_WARN("%s:%d\n", __func__, id);
		return "none";
	}
	return dpss_irq_name_tab[id];
}

typedef irqreturn_t(*DPSS_IRQ_FUNC) (int irq, void *dev_instance);

DPSS_IRQ_FUNC dpss_irq_fun[] = {
	dpss_irq_pre_vs,
	dpss_irq_buf_rls1,
	dpss_irq_buf_rls0,
	dpss_irq_inp,
	dpss_irq_dae2,
	dpss_irq_dae1,
	dpss_irq_dae0,
	dpss_irq_dpe2,
	dpss_irq_dpe1,
	dpss_irq_dpe0,
	dpss_irq_rdma,
	dpss_irq_dbg,
};

/********************************************/
// extern void dpss_frc_prob(struct dpss_dev_s *devp);

void *dpss_get_fw_data(void)
{
	return dpss_get_devp() ? dpss_get_devp()->fw_data : NULL;
}
EXPORT_SYMBOL(dpss_get_fw_data);

void dpss_get_motion(unsigned int *mot)
{
	struct dpss_frc_fw_data_s *pfw_data;
	int i;

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (pfw_data && mot) {
		for (i = 0; i <= 12; i++)
			mot[i] = pfw_data->reg_val[i].val;
	}
}
EXPORT_SYMBOL(dpss_get_motion);

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
static void dpss_early_suspend(struct early_suspend *h)
{
	unsigned int ch = 0;
	struct dpss_ch_s *pch;

	for (ch = 0; ch < DPSS_CHANNEL_NUB; ch++) {
		pch = dpss_get_ch(ch);
		if (pch->c.reg_s1) {
			DBG_WARN("dpss ins no destroy ch:%d\n", ch);
			return;
		}
	}
#ifdef FUNC_EN_PQ
	pq_update_suspend_flag(1);
#endif
	if (dpss_pdev->clk_status) {
		clk_set_rate(dpss_pdev->vpu_clk_dpe, 0);
		clk_disable_unprepare(dpss_pdev->vpu_clk_dae);
		dpss_pdev->clk_status &= ~(1 << 0);
		clk_set_rate(dpss_pdev->vpu_clk_dae, 0);
		clk_disable_unprepare(dpss_pdev->vpu_clk_dpe);
		dpss_pdev->clk_status &= ~(1 << 1);
	}
	dbg_s1("%s ok\n", __func__);
}

static void dpss_late_resume(struct early_suspend *h)
{
	if (!dpss_pdev->clk_status) {
		clk_prepare_enable(dpss_pdev->vpu_clk_dae);
		clk_set_rate(dpss_pdev->vpu_clk_dae, 50000000);
		dpss_pdev->clk_status |= (1 << 0);

		clk_prepare_enable(dpss_pdev->vpu_clk_dpe);
		clk_set_rate(dpss_pdev->vpu_clk_dpe, 50000000);
		dpss_pdev->clk_status |= (1 << 1);
	}

	dpss_irq_set_affinity_hint(dpss_pdev);

	dbg_s1("cur dpe clk :0x%lx\n", clk_get_rate(dpss_pdev->vpu_clk_dpe));
};

static struct early_suspend dpss_early_suspend_handler = {
	.suspend = dpss_early_suspend,
	.resume = dpss_late_resume,
};
#endif

/********************************************/
int dpss_probe(struct platform_device *pdev)
{
	int ret = 0, i;
	struct dpss_dev_s *devp = NULL;
//      int i;
	const struct of_device_id *match;
	struct dpss_data_s *pdata;
	const char *irq_name;

	/* base data */
	dbg_s1("%s:begin ret:%d\n", __func__, ret);
	devp = kzalloc(sizeof(*devp), GFP_KERNEL);
	if (!devp) {
		DBG_ERR("%s fail to allocate memory.\n", __func__);
		goto fail_kmalloc_dev;
	}
	dpss_pdev = devp;
#ifdef RUN_ON_ARM
	/******************/
	ret = alloc_chrdev_region(&devp->devno, 0, DPSS_COUNT, DEVICE_NAME);
	if (ret < 0) {
		DBG_ERR("%s: failed to allocate major number\n", __func__);
		goto fail_alloc_cdev_region;
	}
	dbg_s1("%s: major %d\n", __func__, MAJOR(devp->devno));
	devp->pclss = class_create(CLASS_NAME);
	if (IS_ERR(devp->pclss)) {
		ret = PTR_ERR(devp->pclss);
		DBG_ERR("%s: failed to create class\n", __func__);
		goto fail_class_create;
	}
#endif				//RUN_ON_ARM
	devp->data_l = NULL;
	devp->data_l = kzalloc(sizeof(*devp->data_l), GFP_KERNEL);
	if (!devp->data_l) {
		DBG_ERR("%s fail to allocate data l.\n", __func__);
		goto fail_kmalloc_datal;
	}
#ifdef RUN_ON_ARM
	devp->flags |= DI_SUSPEND_FLAG;
	cdev_init(&devp->cdev, &dpss_fops);
	devp->cdev.owner = THIS_MODULE;
	ret = cdev_add(&devp->cdev, devp->devno, DPSS_COUNT);
	if (ret)
		goto fail_cdev_add;

	devp->devt = MKDEV(MAJOR(devp->devno), 0);
	devp->dev = device_create(devp->pclss, &pdev->dev,
				  devp->devt, devp, "dpss%d", 0);

	if (!devp->dev) {
		DBG_ERR("device_create create error\n");
		goto fail_cdev_add;
	}
	dev_set_drvdata(devp->dev, devp);
	platform_set_drvdata(pdev, devp);
	devp->pdev = pdev;

	/************************/
	match = of_match_device(amlogic_dpss_dt_match, &pdev->dev);
	if (!match) {
		DBG_ERR("%s,no matched table\n", __func__);
		goto fail_cdev_add;
	}
	pdata = (struct dpss_data_s *)devp->data_l;
	pdata->mdata = match->data;

	dbg_s0("name: %s:id[%d]\n", pdata->mdata->name, pdata->mdata->ic_id);

	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret != 0)
		dbg_s1("no reserved mem.\n");
	/*clk enable */
	devp->clk_status = 0x0;
	devp->vpu_clk_dae = devm_clk_get(&pdev->dev, "clk_dae");
	devp->vpu_clk_dpe = devm_clk_get(&pdev->dev, "clk_dpe");
	if (IS_ERR(devp->vpu_clk_dae)) {
		DBG_ERR("%s: get clk dae error.\n", __func__);
	} else {
		clk_prepare_enable(devp->vpu_clk_dae);
		clk_set_rate(devp->vpu_clk_dae, 50000000);
		devp->clk_status |= (1 << 0);
	}
	if (IS_ERR(devp->vpu_clk_dpe)) {
		DBG_ERR("%s: get clk dpe error.\n", __func__);
	} else {
		clk_prepare_enable(devp->vpu_clk_dpe);
		clk_set_rate(devp->vpu_clk_dpe, 50000000);
		devp->clk_status |= (1 << 1);
	}

	int ret2 = tee_write_reg_bits(((DPSS_INP_WR_SEC_SEL << 2) + 0xff850000), 1, 0, 1);

	if (ret2 == 0)
		dbg_h1("8993 wr bit0 success\n");
	else
		DBG_WARN("8993 bit0 %d\n", ret2);

	int ret3 = tee_write_reg_bits(((VPU_DAE_WRAP_SEC_MODE << 2) + 0xff850000), 1, 8, 1);

	if (ret3 == 0)
		dbg_h1("AE6F wr bit8 success\n");
	else
		DBG_WARN("AE6F bit8 %d\n", ret3);

	int ret4 = tee_write_reg_bits(((VPU_DAE_WRAP_SEC_MODE << 2) + 0xff850000), 0, 0, 1);

	if (ret4 == 0)
		dbg_h1("AE6F wr bit0 success\n");
	else
		DBG_WARN("AE6F bit0 %d\n", ret4);

	dpss_cfg_top_dts(devp);
	dpss_mem_prob(devp);

#ifdef FUNC_EN_IRQ
	for (i = 0; i < DPSS_IRQ_ID_NUB; i++) {
		devp->irq_q[i] = -ENXIO;
		irq_name = get_irq_name(i);
		devp->irq_q[i] = platform_get_irq_byname(pdev, irq_name);
		if (devp->irq_q[i] == -ENXIO) {
			DBG_WARN("no %s\n", irq_name);
			continue;
		} else {
			dbg_s2("%s:%d\n", irq_name, devp->irq_q[i]);
		}

		ret = devm_request_irq(&pdev->dev, devp->irq_q[i],
				       dpss_irq_fun[i],
				       IRQF_SHARED, irq_name,
				       (void *)&pdev->dev);
		if (i == 0)
			irq_set_affinity_hint(devp->irq_q[i], cpumask_of(3));
		else
			irq_set_affinity_hint(devp->irq_q[i], cpumask_of(2));
	}

	dbg_s2("irq ok 0404_4k\n");

#endif				/* FUNC_EN_IRQ */

	devp->flags &= (~DI_SUSPEND_FLAG);
	//class
	device_create_file(devp->dev, &dev_attr_config);
	device_create_file(devp->dev, &dev_attr_dump);
	device_create_file(devp->dev, &dev_attr_reg);
#endif				//RUN_ON_ARM
	devp->sema_flg = 1;	/*di_sema_init_flag = 1; */

	dpss_dbg_fs_init();
	dpss_frc_prob(devp);

	dpss_s_prob(devp);
	dpss_prob_other(devp);
	tsk_m_start();
	tsk_s_start();

#ifdef FUNC_EN_COLLING		//CONFIG_AMLOGIC_MEDIA_THERMAL
	register_media_cooling();
#endif
	dpss_v_rdma_prob();
	dpss_rdma_probe();
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	register_early_suspend(&dpss_early_suspend_handler);
#endif
	DBG_INF("%s:ok:\n", __func__);
	return 0;

fail_cdev_add:
	DBG_WARN("%s:fail_cdev_add\n", __func__);
	kfree(devp->data_l);
	devp->data_l = NULL;
fail_kmalloc_datal:
	DBG_WARN("%s:fail_kmalloc datal\n", __func__);
#ifdef RUN_ON_ARM
	/*move from init */
/*fail_pdrv_register:*/
	class_destroy(devp->pclss);
fail_class_create:
	unregister_chrdev_region(devp->devno, DPSS_COUNT);
#endif				//RUN_ON_ARM
fail_alloc_cdev_region:
	kfree(devp);
	devp = NULL;
	dpss_pdev = NULL;
fail_kmalloc_dev:

	return ret;
}

void dpss_destroy_self(void)
{
	int i, ret;
	struct dpss_ch_s *pch;

	dbg_s0("%s:enter\n", __func__);
	for (i = 0; i < DPSS_CHANNEL_NUB; i++) {
		pch = dpss_get_ch(i);
		if (pch && pch->c.reg_s1) {
			dbg_s0("%s:%d\n", __func__, i);
			ret = dpss_destroy_instance(i);
			if (ret != DPSS_ERR_NONE)
				DBG_ERR("%s:%d\n", __func__, ret);
		}
	}
}

#ifdef RUN_ON_ARM
static void dpss_remove(struct platform_device *pdev)
{
	struct dpss_dev_s *devp = NULL;

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	unregister_early_suspend(&dpss_early_suspend_handler);
#endif
	dbg_s0("%s:\n", __func__);
	devp = platform_get_drvdata(pdev);

	dpss_v_rdma_remove();
	dpss_s_remove(devp);

	dpss_frc_remove();

	tsk_m_stop();
	tsk_s_stop();

	dpss_dbg_fs_exit();

	if (dpss_pdev->clk_status) {
		clk_disable_unprepare(devp->vpu_clk_dae);
		devp->clk_status &= ~(1 << 0);
		clk_disable_unprepare(devp->vpu_clk_dpe);
		devp->clk_status &= ~(1 << 1);
	}
	/* Remove the cdev class */
	device_remove_file(devp->dev, &dev_attr_config);
	device_remove_file(devp->dev, &dev_attr_dump);
	device_remove_file(devp->dev, &dev_attr_reg);

	cdev_del(&devp->cdev);

	dpss_mem_remove(devp);

	device_destroy(devp->pclss, devp->devno);

/* free drvdata */
	dev_set_drvdata(&pdev->dev, NULL);
	platform_set_drvdata(pdev, NULL);

	/*move to remove */
	class_destroy(devp->pclss);

	//to-do dim_debugfs_exit();

	dpss_exit();
	unregister_chrdev_region(devp->devno, DPSS_COUNT);

	kfree(devp->data_l);
	devp->data_l = NULL;

	kfree(devp);
	devp = NULL;
	dpss_pdev = NULL;
#ifdef FUNC_EN_COLLING		//CONFIG_AMLOGIC_MEDIA_THERMAL
	unregister_media_cooling();
#endif
	dbg_s0("%s:finish\n", __func__);
}

static void dpss_shutdown(struct platform_device *pdev)
{
	struct dpss_dev_s *devp = NULL;

	devp = platform_get_drvdata(pdev);

	if (!devp) {
		DBG_ERR("%s:no devp\n", __func__);
		return;
	}
	devp->flags |= DI_SUSPEND_FLAG;

	dpss_destroy_self();

	if (dpss_pdev->clk_status) {
		clk_disable_unprepare(devp->vpu_clk_dae);
		devp->clk_status &= ~(1 << 0);
		clk_disable_unprepare(devp->vpu_clk_dpe);
		devp->clk_status &= ~(1 << 1);
	}
	dpss_s_shutdown(devp);
	dbg_s0("%s.\n", __func__);
}

#ifdef CONFIG_PM

static int dpss_freeze(struct device *dev)
{
	struct dpss_dev_s *devp = NULL;

	devp = dev_get_drvdata(dev);

	dpss_s_freeze(devp);

	return 0;
}

static int dpss_thaw(struct device *dev)
{
	struct dpss_dev_s *devp = NULL;

	devp = dev_get_drvdata(dev);

	dpss_s_thaw(devp);
	devp->flags &= ~DI_SUSPEND_FLAG;

	/************/
	dbg_s0("%s finish\n", __func__);
	return 0;
}

static int dpss_restore(struct device *dev)
{
	struct dpss_dev_s *devp = NULL;

	devp = dev_get_drvdata(dev);

	dpss_s_restore(devp);
	devp->flags &= ~DI_SUSPEND_FLAG;

	/************/
	dbg_s0("%s finish\n", __func__);
	return 0;
}

/* must called after lcd */
static int dpss_suspend(struct device *dev)
{
	struct dpss_dev_s *devp = NULL;

	devp = dev_get_drvdata(dev);
	dpss_s_suspend(devp);

	dbg_s0("%s finish\n", __func__);
	return 0;
}

/* must called before lcd */
static int dpss_resume(struct device *dev)
{
	struct dpss_dev_s *devp = NULL;

	dbg_s0("%s\n", __func__);
	devp = dev_get_drvdata(dev);

	dpss_s_resume(devp);
	devp->flags &= ~DI_SUSPEND_FLAG;
	/************/
	dpss_frc_resume();
	dbg_s0("%s finish\n", __func__);
	return 0;
}

static const struct dev_pm_ops dpss_pm_ops = {
	.suspend_late = dpss_suspend,
	.resume_early = dpss_resume,
	.freeze = dpss_freeze,
	.restore = dpss_restore,
	.thaw = dpss_thaw,
};
#endif

static struct platform_driver dpss_driver = {
	.probe = dpss_probe,
	.remove = dpss_remove,
	.shutdown = dpss_shutdown,
	.driver = {
		   .name = DEVICE_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = amlogic_dpss_dt_match,
#ifdef CONFIG_PM
		   .pm = &dpss_pm_ops,
#endif
		}
};

int __init dpss_module_init(void)
{
	int ret = 0;

	pr_debug("dpss:%s\n", __func__);

	ret = platform_driver_register(&dpss_driver);
	if (ret != 0) {
		DBG_ERR("%s: failed to register driver\n", __func__);
		/*goto fail_pdrv_register; */
		return -ENODEV;
	}
	pr_debug("%s finish\n", "dpss init");
	return 0;
}

void __exit dpss_module_exit(void)
{
	platform_driver_unregister(&dpss_driver);
	pr_debug("%s: ok.\n", __func__);
}
#endif				//RUN_ON_ARM

//MODULE_DESCRIPTION("AMLOGIC DPSS driver");
//MODULE_LICENSE("GPL");
//MODULE_VERSION("4.0.0");
