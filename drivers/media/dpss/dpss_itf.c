// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include "sys_def.h" //ary add for sim
#ifdef RUN_ON_ARM
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/kfifo.h>
//#include <linux/sched/clock.h>
#include <linux/delay.h> //usleep_range
#include <linux/vmalloc.h>

#include <linux/amlogic/media/di/dpss_interface.h>
#include <linux/amlogic/media/dpss/dpss_frc.h>
#include <linux/amlogic/media/codec_mm/codec_mm_prealloc.h>
#include <linux/amlogic/media/vout/vout_notify.h>

#include <linux/clk.h>
#include <linux/clk-provider.h>
#endif /* RUN_ON_ARM */

#include "dpss_base.h"
#include "dpss_s.h"
#include "dpss_sys.h"
#include "dpss_func.h"
#include "dpss_input_q.h"

bool dpss_bypass_all;
module_param_named(dpss_bypass_all, dpss_bypass_all, bool, 0664);

/*game frc*/
#define GAME_RES_LIST_MAX 3
#define GAME_N2M_LIST_MAX 4
#define DPSS_FRC_RATIO_1_1  0
#define DPSS_FRC_RATIO_1_2  1
// #define DPSS_FRC_RATIO_1_4  2

static struct dpss_frc_game_res_t frc_res_list[GAME_RES_LIST_MAX] = {
	{1280, 720},
	{1920, 1080},
	//{2560, 1440},
	{3840, 2160},
};

static struct dpss_game_n2m_t frc_n2m_list[GAME_N2M_LIST_MAX] = {
	// {25,  50},
	// {30,  60},
	// {60,  60},
	{50,  100},
	{60,  120},
	{100, 100},
	{120, 120},
};

/*game end*/

unsigned int dpss_force_v;
module_param_named(dpss_force_v, dpss_force_v, uint, 0664);

static void _ch_releas_data_fifo(struct dpss_ch_d_s1 *pch_d)
{
	int i;
	struct dpss_fifo_s *q;

	if (!pch_d)
		return;

	for (i = 0; i < EDPSS_Q_CH_NUB; i++) {
		q = &pch_d->q_ch[i];
		if (q->flg_alloc)
			kfifo_free(&q->f);
	}
}

static void read_back_reg_d410(struct dpss_ch_s *pch)
{
	if (!pch) {
		DBG_ERR("%s:no pch\n", __func__);
		return;
	}

	if (pch->c.in_cnt > 2)
		dpss_ei_sel = rd(VPU_DI_BLEND_EI_POST_EN_MODE);
	else
		dpss_ei_sel = 0;
}

bool dpss_api_unreg(struct dpss_ch_s *pch)
{
	bool ret = false;
	unsigned int cnt;

	if (!pch) {
		DBG_ERR("%s:no pch\n", __func__);
		return true;
	}
	if (!pch->d || !pch->c.reg_s1) {
		DBG_ERR("%s:no need unreg:0x%px, %d\n",
			__func__, pch->d, pch->c.reg_s1);
		return true;
	}

	read_back_reg_d410(pch);
	if (!pch->c.reg_s2) {
		dbg_i0("%s:trig0\n", __func__);
//		atomic_set(&pch->c.trig_reg, 0);
//		return true;
	}
	dbg_ins0("%s:begin:%d\n", __func__, pch->c.ch);
	/* trig unreg */
	atomic_set(&pch->c.trig_unreg, 1);

	dpss_tsk_m_delay(100);
	dpss_tsk_m_wake(6);

	cnt = 0; /* 500us x 10 = 5ms */
	while (atomic_read(&pch->c.trig_unreg) && cnt < 10) {
		usleep_range(500, 501);
		cnt++;
	}

	cnt = 0; /* 3ms x 2000 = 6s */
	while (atomic_read(&pch->c.trig_unreg) && cnt < 2000) {
		usleep_range(3000, 3001);
		if (!(cnt & 0xf)) {
			dpss_tsk_m_wake(6);
			dbg_ins2("u_t=%d\n", cnt);
		}
		cnt++;
	}

	if (!atomic_read(&pch->c.trig_unreg))
		ret = true;
	else
		DBG_ERR("%s:ch[%d]:failed\n", __func__, pch->c.ch);

	dbg_ins0("%s:ch[%d],%d\n", __func__, pch->c.ch, cnt);

	return ret;
}

/*called by other thread */
bool dpss_api_reg(struct dpss_ch_s *pch)	//dim_api_reg
{
	bool ret = true;

	if (!pch)
		return false;
	//trig reg
	atomic_set(&pch->c.trig_reg, 1);
	//no need to wait
	dpss_tsk_m_delay(50);

	dpss_tsk_m_wake(1);

	return ret;
}

const struct dpss_init_parm vfm_path_parm = {
	.dps_work_mode = DPSS_WORK_MODE_NR,
	.di_parm = {
		.buffer_mode = DPSS_BUFFER_MODE_ALLOC_FAST,
		.output_format = DPSS_OUTPUT_BY_DI_DEFINE,
	},
};

const struct dpss_init_parm vfm_frc_path_parm = {
	.dps_work_mode = DPSS_WORK_MODE_FRC,
	.frc_parm = {
		.buffer_mode = DPSS_BUFFER_MODE_ALLOC_FAST,
	},
};

/* keep order with EDPSS_Q_CH_ID */
const char * const ch_q_name[] = {
	"ch_idle_nr",
	"ch_idle_frc",
	"ch_in",
	"ch_rd",
	"ch_in_recycle",
	"ch_back",
	"ch_v_idle",
	"ch_v_recycle",
	"ch_nr_doing",
	"ch_frc_doing",
	"ch_front_idle",
	"ch_front_o",
	"ch_buf",
};

static const char *_get_ch_q_name(unsigned int index)
{
	if (index >= EDPSS_Q_CH_NUB || index >= ARRAY_SIZE(ch_q_name))
		return "nothing!";

	return ch_q_name[index];
}

bool dpss_q_in_idx(struct dpss_fifo_s *q, unsigned char val)
{
	int ret_fifo;

	if (!q)
		return false;
	dbg_s2("%s:%s,val:%d\n",
			__func__,
			q->name,
			val);
	ret_fifo = kfifo_put(&q->f, val);
	if (ret_fifo != 1) {
		DBG_ERR("%s:%s,%d:can't to idle\n",
			__func__,
			q->name,
			val);
		return false;
	}
	return true;
}

static struct vframe_s *dpss_id_2_vfm(struct dpss_ch_s *pch, unsigned char val)
{
	struct vframe_s *vfm;

	dbg_i2("%s:val=%d\n", __func__, val);
	if (val < EDPSS_HD_ID_NR_BEGIN + DPSS_VFM_IN_NUB) {
		vfm = &pch->d->vfm_nr[val];
		return vfm;
	}

	if (val < (EDPSS_HD_ID_BYPASS_BEGIN + DPSS_VFM_IN_NUB) &&
	    val > EDPSS_HD_ID_BYPASS_BEGIN) {
		vfm = pch->d->vfm_bp[val - EDPSS_HD_ID_BYPASS_BEGIN];
		return vfm;
	}
	if (val >= EDPSS_HD_ID_FRC_BEGIN &&
	    val < (EDPSS_HD_ID_FRC_BEGIN + DPSS_VFM_FRC_NUB)) {
		vfm = &pch->d->vfm_frc[val - EDPSS_HD_ID_FRC_BEGIN];
//		DBG_INF("%s:val=%d,0x%px\n", __func__, val, vfm);
		return vfm;
	}
	return NULL;
}

struct vframe_s *dpss_q_out_vfm(struct dpss_ch_s *pch, unsigned int q_id)
{
	struct dpss_fifo_s *q;
	int ret_fifo;
	unsigned char val;
	struct vframe_s *vfm;

	if (q_id >= EDPSS_Q_CH_NUB || !pch->d) {
		DBG_ERR("%s:q_id=%d,0x%px\n",
			__func__, q_id, pch->d);
		return NULL;
	}
	q = &pch->d->q_ch[q_id];
	if (!q)
		return NULL;

	ret_fifo = kfifo_get(&q->f, &val);
	if (ret_fifo != 1)
		return NULL;

	vfm = dpss_id_2_vfm(pch, val);
	if (vfm)
		return vfm;

	DBG_ERR("%s:q_id=%d,%d\n", __func__, q_id, val);
	return NULL;
}

struct vframe_s *dpss_q_peek_vfm(struct dpss_ch_s *pch, unsigned int q_id)
{
	struct dpss_fifo_s *q;
	int ret_fifo;
	unsigned char val;
	struct vframe_s *vfm;

	if (q_id >= EDPSS_Q_CH_NUB || !pch->d) {
		DBG_ERR("%s:q_id=%d,0x%px\n",
			__func__, q_id, pch->d);
		return NULL;
	}
	q = &pch->d->q_ch[q_id];
	if (!q) {
		DBG_ERR("%s:no q? %d\n", __func__, q_id);
		return NULL;
	}

	ret_fifo = kfifo_peek(&q->f, &val);
	if (ret_fifo != 1)
		return NULL;

	vfm = dpss_id_2_vfm(pch, val);
	if (vfm)
		return vfm;

	DBG_ERR("%s:q_id=%d,%d\n", __func__, q_id, val);
	return NULL;
}

unsigned int dpss_q_get_len(struct dpss_ch_s *pch, unsigned int q_id)
{
	struct dpss_fifo_s *q;
//	int ret_fifo;
//	unsigned char val;
//	struct vframe_s *vfm;
	unsigned int len;

	if (q_id >= EDPSS_Q_CH_NUB || !pch->d) {
		DBG_ERR("%s:q_id=%d,0x%px\n",
			__func__, q_id, pch->d);
		return 0;
	}
	q = &pch->d->q_ch[q_id];
	if (!q) {
		DBG_ERR("%s:no q? %d\n", __func__, q_id);
		return 0;
	}
	len = kfifo_len(&q->f);
	return len;
}

unsigned char lk_owner_get(struct vframe_s *vfm)
{
	struct dpss_lk1_s *lk;

	if (!vfm->dpss_data)
		return DPSS_VF_OWNER_ILLEGAL;
	lk = (struct dpss_lk1_s *)vfm->dpss_data;
	if (lk->idx_t >= DPSS_VF_OWNER_ILLEGAL)
		DBG_WARN("%s:illegal %p %p\n", __func__, vfm, lk);

	return lk->idx_t;
}

unsigned char lk_idx_get(struct vframe_s *vfm)
{
	struct dpss_lk1_s *lk;

	if (!vfm->dpss_data)
		return 0xff;
	lk = (struct dpss_lk1_s *)vfm->dpss_data;

	return lk->idx_q;
}

struct dpss_nr_i_s *lk_get_nr_i(struct vframe_s *vfm)
{
	struct dpss_lk1_s *lk;
	struct dpss_nr_i_s *nr_inf;

	if (!vfm->dpss_data) {
		DBG_WARN("%s:no dpss_data %p\n", __func__, vfm);
		return NULL;
	}
	lk = (struct dpss_lk1_s *)vfm->dpss_data;
	if (!lk->lk_up || !lk->lk_dn || !lk->lk_info)
		return NULL;

	if (lk->idx_t != DPSS_VF_OWNER_NR) {
		dbg_h2("not nr? (%u)\n", lk->idx_t);
		return NULL;
	}
	nr_inf = (struct dpss_nr_i_s *)lk->lk_info;
	dbg_i2("%s:%p\n", __func__, nr_inf);

	return nr_inf;
}

struct dpss_frc_i_s *lk_get_frc_i(struct vframe_s *vfm)
{
	struct dpss_lk1_s *lk;
	struct dpss_frc_i_s *frc_inf;

	if (!vfm->dpss_data) {
		DBG_WARN("%s:no dpss_data %p\n", __func__, vfm);
		return NULL;
	}
	lk = (struct dpss_lk1_s *)vfm->dpss_data;
	if (!lk->lk_up || !lk->lk_dn || !lk->lk_info)
		return NULL;

	if (lk->idx_t != DPSS_VF_OWNER_FRC) {
		DBG_WARN("%s:not frc? (%u)\n", __func__, lk->idx_t);
		return NULL;
	}
	frc_inf = (struct dpss_frc_i_s *)lk->lk_info;
	dbg_i2("%s:%p\n", __func__, frc_inf);

	return frc_inf;
}

bool dpss_q_in_vfm(struct dpss_fifo_s *q, struct vframe_s *vfm)
{
	unsigned int val;
	bool ret;

	val = lk_idx_get(vfm);
	if (val == 0xff)
		return false;

	ret = dpss_q_in_idx(q, val);
	return ret;
}

void dpss_in_rck_in(struct dpss_ch_s *pch, struct vframe_s *vfm)
{
	struct dpss_fifo_s *q;
	struct dpss_lk1_s *lk;
	unsigned char val;
	int ret_fifo;

	q = &pch->d->q_ch[EDPSS_Q_CH_V_IDLE];

	ret_fifo = kfifo_get(&q->f, &val);
	if (ret_fifo != 1) {
		DBG_ERR("%s: no idle\n", __func__);
		return;
	}
	if (val < EDPSS_HD_ID_BYPASS_BEGIN ||
	    val >= EDPSS_HD_ID_FRC_BEGIN) {
		DBG_ERR("%s: overflow:%d\n", __func__, val);
		return;
	}
	if (val - EDPSS_HD_ID_BYPASS_BEGIN >= 20) {
		DBG_ERR("%s: array overflow:%d\n", __func__, val);
		return;
	}
	lk = &pch->d->lk_bp[val - EDPSS_HD_ID_BYPASS_BEGIN];
	lk->lk_info = vfm;
	vfm->dpss_data = lk;

	q = &pch->d->q_ch[EDPSS_Q_CH_V_RECYCLE];
	ret_fifo = kfifo_put(&q->f, val);
	if (ret_fifo != 1) {
		DBG_ERR("%s: no rec\n", __func__);
		return;
	}
}

struct vframe_s *dpss_in_rck_o(struct dpss_ch_s *pch)
{
	struct dpss_fifo_s *q;
	int ret_fifo;
	unsigned char val;
	struct vframe_s *vfm;
	struct dpss_lk1_s *lk;

	q = &pch->d->q_ch[EDPSS_Q_CH_V_RECYCLE];
	if (!kfifo_len(&q->f))
		return NULL;

	ret_fifo = kfifo_get(&q->f, &val);
	if (ret_fifo != 1) {
		DBG_ERR("%s: no rcl\n", __func__);
		return NULL;
	}
	if (val < EDPSS_HD_ID_BYPASS_BEGIN ||
	    val >= EDPSS_HD_ID_FRC_BEGIN) {
		DBG_ERR("%s: overflow:%d\n", __func__, val);
		return NULL;
	}
	if (val - EDPSS_HD_ID_BYPASS_BEGIN >= 20) {
		DBG_ERR("%s: array overflow:%d\n", __func__, val);
		return NULL;
	}
	lk = &pch->d->lk_bp[val - EDPSS_HD_ID_BYPASS_BEGIN];
	if (!lk->lk_info) {
		DBG_ERR("%s: no vfm\n", __func__);
		return NULL;
	}
	vfm = (struct vframe_s *)lk->lk_info;
	lk->lk_info = NULL;
	vfm->dpss_data = NULL;
	q = &pch->d->q_ch[EDPSS_Q_CH_V_IDLE];
	ret_fifo = kfifo_put(&q->f, val);
	if (ret_fifo != 1) {
		DBG_ERR("%s: idle\n", __func__);
		return NULL;
	}
	return vfm;
}

/*called by other thread */
bool dpss_api_init_data(struct dpss_ch_s *pch)
{
//	bool ret = true;
	struct dpss_ch_d_s1 *pch_d;
	int ret_fifo;
	unsigned int err_cnt = 0;
	int i;
	struct dpss_fifo_s *q;

	struct vframe_s *vfm;
	unsigned int code_ins;

	struct dpss_lk1_s *lk;
	struct dpss_nr_i_s *nr_i;
	struct dpss_frc_i_s *frc_i;

	if (!pch->c.reg_s1) {
		DBG_ERR("%s:ch[%d]not reg?\n", __func__, pch->c.ch);
		return false;
	}

	if (pch->d) {
		DBG_WARN("%s:ch[%d] has data?\n", __func__, pch->c.ch);
		vfree(pch->d);
		pch->d = NULL;
	}

	//pch_d = kzalloc(sizeof(*pch_d), GFP_KERNEL);
	pch_d = vmalloc(sizeof(*pch_d));

	if (!pch_d) {
		DBG_ERR("%s fail to allocate memory.\n", __func__);
		goto dpss_pch_d_fail1;
	}
	memset(pch_d, 0, sizeof(struct dpss_ch_d_s1));
	/* que init */
	for (i = 0; i < EDPSS_Q_CH_NUB; i++) {
		q = &pch_d->q_ch[i];
		ret_fifo = FIFO_32_ALLOC(&q->f);
		if (ret_fifo < 0) {
			q->flg_alloc = false;
			err_cnt++;
			break;
		}
		q->flg_alloc = true;

		q->id = i;
		q->name = _get_ch_q_name(i);
	}

	if (err_cnt) {
		DBG_ERR("%s:%d:\n", __func__, err_cnt);
		goto dpss_pch_d_q_fail;
	}

	if (pch->c.parm.dps_work_mode & DPSS_WORK_MODE_FRONT)
		pch_d->di_front = true;
	dbg_i0("ch[%d] di_front=%d\n", pch->c.ch, pch_d->di_front);
	/* new link */
	code_ins = code_count(pch);
	for (i = 0; i < DPSS_VFM_IN_NUB; i++) {
		nr_i = &pch_d->nr_i[i];
		nr_i->idx = i;
		vfm = &pch_d->vfm_nr[i];

		lk = &pch_d->lk_nr[i];
		lk->idx_t = DPSS_VF_OWNER_NR;
		lk->idx_q = EDPSS_HD_ID_NR_BEGIN + i;
		lk->lk_info = nr_i;
		nr_i->lk_parent = lk;
		lk->lk_dn = pch;
		lk->code_if = code_count(&pch_d->nr_i[i]);
		lk->code_dn = code_ins;
		lk->lk_up = vfm;
		vfm->dpss_data = lk;
		lk->code_up = code_count(vfm);
		nr_i->lk_grd_parent = vfm;
		dbg_i2("%d:vfm:%px\n", i, vfm);
		dbg_i0("\tnr_i:%px\n", nr_i);
		//bp
		lk = &pch_d->lk_bp[i];
		lk->idx_t = DPSS_VF_OWNER_OTHER;
		lk->idx_q = EDPSS_HD_ID_BYPASS_BEGIN + i;
		lk->lk_info = NULL;
		lk->lk_dn = pch;
		lk->code_dn = code_ins;
		q = &pch_d->q_ch[EDPSS_Q_CH_V_IDLE];
		dpss_q_in_idx(q, lk->idx_q);
		//to do frc?
		frc_i = &pch_d->frc_i[i];
		frc_i->idx = i;
		vfm = &pch_d->vfm_frc[i];

		lk = &pch_d->lk_frc[i];
		lk->idx_t = DPSS_VF_OWNER_FRC;
		lk->idx_q = EDPSS_HD_ID_FRC_BEGIN + i;
		lk->lk_info = frc_i;
		frc_i->lk_parent = lk;
		lk->lk_dn = pch;
		lk->code_if = code_count(&pch_d->frc_i[i]);
		lk->code_dn = code_ins;
		lk->lk_up = vfm;
		vfm->dpss_data = lk;
		lk->code_up = code_count(vfm);
		//nr_i->lk_grd_parent = vfm;
		dbg_i2("%d:vfm:%px\n", i, vfm);
		dbg_i0("\tfrc_i:%px\n", frc_i);
		//q in idle:
		q = &pch_d->q_ch[EDPSS_Q_CH_IDLE_NR];
		dpss_q_in_idx(q, i);
		q = &pch_d->q_ch[EDPSS_Q_CH_IDLE_FRC];
		dpss_q_in_idx(q, EDPSS_HD_ID_FRC_BEGIN + i);
	}
	pch->d = pch_d;

	//di_front
	if (pch->d->di_front) {
		q = &pch_d->q_ch[EDPSS_Q_CH_FRONT_IDLE];
		for (i = 0; i < DPSS_CFG_NUM; i++) {
			if (kfifo_is_full(&q->f)) {
				DBG_WARN("di_front q idle is full %d\n", i);
				break;
			}
			dpss_q_in_idx(q, i);
		}
	}
	return true;

dpss_pch_d_q_fail:
	_ch_releas_data_fifo(pch_d);
dpss_pch_d_fail1:
	vfree(pch_d);
	pch->d = NULL;
	return false;
}

#ifdef _HIS_CODE_
static bool _reg_idle_ch(unsigned int *ch_return)
{
	unsigned int ch;
	bool ret = false;

	//int i;
	struct dpss_ch_s *pch;

	for (ch = 0; ch < DPSS_CHANNEL_NUB; ch++) {
		pch = dpss_get_ch(ch);

		mutex_lock(&pch->c.ch_lock);

		if (!pch->c.reg_s1) {
			/*set reg flg*/
			pch->c.reg_s1 = true;
			*ch_return = ch;
			ret = true;
			mutex_unlock(&pch->c.ch_lock);
			break;
		}

		mutex_unlock(&pch->c.ch_lock);
	}
	dbg_i1("%s:%d, %d\n", __func__, ret, *ch_return);

	return ret;
}
#endif

static bool _reg_ch_idle(unsigned int ch)
{
//	unsigned int ch;
	bool ret = false;

	//int i;
	struct dpss_ch_s *pch;

	pch = dpss_get_ch(ch);

	mutex_lock(&pch->c.ch_lock);

	if (!pch->c.reg_s1) {
		/*set reg flg*/
		pch->c.reg_s1 = true;
		ret = true;
	}

	mutex_unlock(&pch->c.ch_lock);

	return ret;
}

/**********************************************************
 * @brief  di_create_instance  creat di instance
 * @param[in]  parm    Pointer of parm structure
 * @return      di index for success, or fail type if < 0
 **********************************************************/
int _create_instance(bool fix, int index, struct dpss_init_parm *parm)
{
	bool reg_idle;
	bool is_4k, is_fhd_high_fps;
	unsigned int ch = 0;
	struct dpss_ch_s *pch;
	struct dpss_dev_s *dpss_pdev;

	dpss_pdev = dpss_get_devp();

	dbg_ins0("%s:fix:%d, index:%d\n", "create", fix, index);

	clk_set_rate(dpss_pdev->vpu_clk_dpe, 800000000);
	clk_set_rate(dpss_pdev->vpu_clk_dae, 800000000);
	if (fix) {
		ch = (unsigned int)index;
		if (ch >= DPSS_CHANNEL_NUB)
			return DPSS_ERR_INDEX_OVERFLOW;

	} else {
		if (parm->dps_work_mode & DPSS_WORK_MODE_MAIN ||
			parm->dps_work_mode == DPSS_WORK_MODE_FRC)
			ch = 0;
		else
			ch = 1;

		reg_idle = _reg_ch_idle(ch);
		if (!reg_idle) {
			DBG_ERR("%s:no idle ch[%d]:0x%x\n", __func__, ch, parm->dps_work_mode);
			return DPSS_ERR_REG_NO_IDLE_CH;
		}
	}
	pch = dpss_get_ch(ch);
	if (fix) {
		pch->c.etype = 0;
		pch->c.reg_s1 = true;
	} else {
		pch->c.etype = 1;
	}
	/*parm*/
	memcpy(&pch->c.parm, parm, sizeof(pch->c.parm));
	//reg:
	mutex_lock(&pch->c.ch_lock);

	if (parm->dps_work_mode != DPSS_WORK_MODE_FRC &&
				fix == 0) { //	not for frc_only case
		is_4k = (pch->c.parm.di_parm.width > 1920 || pch->c.parm.di_parm.height > 1088);

		is_fhd_high_fps = (pch->c.parm.di_parm.width == 1920 &&
			pch->c.parm.di_parm.duration <= 960 &&
			pch->c.parm.di_parm.duration > 0 &&
			pch->c.parm.di_parm.is_interlace == 0 &&
			ch == 0);

		if (is_4k || is_fhd_high_fps)
			pch->c.o_afbc = 2;
		else
			pch->c.o_afbc = 0;

		if ((dpss_en_afbc_force & 0xff) &&
				ch == 0) {
			if (dpss_en_afbc_force & C_BIT0)
				pch->c.o_afbc = 1;
			else if (dpss_en_afbc_force & C_BIT1)
				pch->c.o_afbc = 2;
			else  if (dpss_en_afbc_force & C_BIT2)
				pch->c.o_afbc = 0;
		} else if ((dpss_en_afbc_force & 0xff00) &&
				ch == 1) {
			if (dpss_en_afbc_force & C_BIT8)
				pch->c.o_afbc = 1;
			else if (dpss_en_afbc_force & C_BIT9)
				pch->c.o_afbc = 2;
			else if (dpss_en_afbc_force & C_BIT10)
				pch->c.o_afbc = 0;
		}
	}

	if (parm->dps_work_mode == DPSS_WORK_MODE_FRC)
		dpss_game_frc_lock(parm);

	if (dpss_nr_debug == 1)
		pch->c.o_afbc = 0;

	if (!dpss_api_init_data(pch)) {
		pch->c.reg_s1 = false;
		mutex_unlock(&pch->c.ch_lock);
		DBG_ERR("%s:init data err ch[%d]\n", __func__, ch);
		return DPSS_ERR_IN_NO_SPACE;
	}

	dpss_api_reg(pch);
	mutex_unlock(&pch->c.ch_lock);
	dbg_ins0("%s:ch[%d],tmode[0x%x],out[0x%x] o_afbc=%d\n", "ins:create",
		ch, parm->dps_work_mode,
		parm->di_parm.output_format,
		pch->c.o_afbc);
	if (parm->dps_work_mode != DPSS_WORK_MODE_FRC &&
				fix == 0 &&
				!pch->d->di_front) // not for frc only case
		dpss_input_q_init(&pch->q);
	return (int)ch;
}

static enum DPSS_ERRORTYPE _destroy_instance(struct dpss_ch_s *pch)
{
	struct dpss_dev_s *dpss_pdev;

	dpss_pdev = dpss_get_devp();
	if (!pch) {
		DBG_WARN("%s:NULL param.\n", __func__);
		return DPSS_ERR_INDEX_OVERFLOW;
	}

	if (!pch->d || !pch->c.reg_s1) {
		DBG_WARN("%s:no ins? %d\n", __func__, pch->c.reg_s1);
		return DPSS_ERR_INDEX_OVERFLOW;
	}
	dpss_api_unreg(pch);
	_ch_releas_data_fifo(pch->d);
	//dpss-patch for reg
	if (pch->d)
		vfree(pch->d);
	pch->d = NULL;
	pch->c.reg_s1 = 0;
	dbg_ins0("destroy:ch[%d]\n", pch->c.ch);
	if (!pch->c.ch) { //2026-02-05 tmp
		clk_set_rate(dpss_pdev->vpu_clk_dpe, 50000000);
		clk_set_rate(dpss_pdev->vpu_clk_dae, 50000000);
	}

	return DPSS_ERR_NONE;
}

void itf_get_status(struct dpss_ch_s *pch, struct vframe_s *vfm,
			struct dpss_in_vf_info *inf_in)
{
	struct dpss_operations_s *op;
	enum DPSS_ERRORTYPE ret;

	if (pch->c.etype) {
		op = &pch->c.parm.ops;
		ret = op->get_input_vf_info(op->arg, vfm, inf_in);
		if (ret != DPSS_ERR_NONE)
			DBG_WARN("%s:%p\n", __func__, vfm);

		dbg_i2("%s:m_idx=%d\n", __func__, inf_in->idx_m);
	}
}

void itf_clear_caller_data(struct dpss_ch_s *pch, struct vframe_s *vfm)
{
	struct dpss_operations_s *op;
	enum DPSS_ERRORTYPE ret;

	if (pch->c.etype) {
		op = &pch->c.parm.ops;
		if (!op->clear_data)
			return;

		ret = op->clear_data(op->arg, vfm);
		if (ret != DPSS_ERR_NONE)
			DBG_WARN("%s:%p\n", __func__, vfm);
	}
}

enum DPSS_ERRORTYPE _input_buffer(struct dpss_ch_s *pch, struct vframe_s *vfm)
{
	//get idle:
	struct vframe_s *vfm_idle;
	bool ret;
	struct canvas_s cvs_in;
	unsigned int cvs_id;
	struct dpss_nr_i_s *nr_i;
	struct dpss_frc_i_s *frc_i;
	unsigned int len_nr, len_in, len_frc;
	unsigned int alig_height;

	if (!pch || !pch->d) {
		DBG_ERR("%s 01\n", __func__);
		return DPSS_ERR_INDEX_NOT_ACTIVE;
	}

	if (pch->c.parm.dps_work_mode != DPSS_WORK_MODE_FRC) {
		len_nr = kfifo_len(&pch->d->q_ch[EDPSS_Q_CH_IDLE_NR].f);
		len_in = kfifo_len(&pch->d->q_ch[EDPSS_Q_CH_IN].f);
		if (!len_in && !len_nr) {
			DBG_WARN("%s:no space:%d,%d\n", __func__, len_in, len_nr);
			return DPSS_ERR_IN_NO_SPACE;
		}
		vfm_idle = dpss_q_out_vfm(pch, EDPSS_Q_CH_IDLE_NR);
		if (!vfm_idle || !vfm_idle->dpss_data) {
			DBG_ERR("%s:no idle:len= %d\n", __func__,
				kfifo_len(&pch->d->q_ch[EDPSS_Q_CH_IDLE_NR].f));
			return DPSS_ERR_IN_NO_SPACE;
		}
		dbg_i2("in:idle:vfm= %p\n", vfm_idle);
		nr_i = lk_get_nr_i(vfm_idle);
		dbg_i2("in:nr_i = %p\n", nr_i);
		if (!nr_i) {
			DBG_ERR("%s illegal %p\n", __func__, vfm_idle);
			return DPSS_ERR_UNDEFINED;
		}

		if (dpss_force_v) {
			if (VFM_IS_COMP_MODE(vfm->type)) {
				vfm->compHeight = dpss_force_v;
				vfm->height = dpss_force_v / 4;
			} else {
				vfm->height = dpss_force_v;
			}
		}
		nr_i->in_vfm = vfm;
		if (check_need_enable_4k1k(nr_i->in_vfm)) {
			if (VFM_IS_COMP_MODE(vfm->type)) {
				if (nr_i->in_vfm->compHeight % 2)
					nr_i->in_vfm->compHeight =
						roundup(nr_i->in_vfm->compHeight, 2);
				alig_height = nr_i->in_vfm->compHeight >> 1;
				if (alig_height % 2)
					alig_height = rounddown(alig_height, 2);
				nr_i->in_vfm->compHeight = alig_height * 2;
			} else {
				if (nr_i->in_vfm->height % 2)
					nr_i->in_vfm->height = roundup(nr_i->in_vfm->height, 2);
				alig_height = nr_i->in_vfm->height >> 1;
				if (alig_height % 2)
					alig_height = rounddown(alig_height, 2);
				nr_i->in_vfm->height = alig_height * 2;
			}
		}
		if (pch->c.etype) { //ins path
			itf_get_status(pch, vfm, &nr_i->info_in);
		} else {
			nr_i->info_in.idx_m = pch->d->cnt_pre_get;
		}
	} else {
		// USE_FRC_ONLY_CASE
		len_frc = kfifo_len(&pch->d->q_ch[EDPSS_Q_CH_IDLE_FRC].f);
		len_in = kfifo_len(&pch->d->q_ch[EDPSS_Q_CH_IN].f);
		if (!len_in && !len_frc) {
			DBG_WARN("%s:no space:%d,%d\n", __func__, len_in, len_frc);
			return DPSS_ERR_IN_NO_SPACE;
		}
		vfm_idle = dpss_q_out_vfm(pch, EDPSS_Q_CH_IDLE_FRC);
		if (!vfm_idle || !vfm_idle->dpss_data) {
			DBG_ERR("%s:no idle:len= %d\n", __func__,
				kfifo_len(&pch->d->q_ch[EDPSS_Q_CH_IDLE_FRC].f));
			return DPSS_ERR_IN_NO_SPACE;
		}
		dbg_i2("in:idle:vfm= %p\n", vfm_idle);
		frc_i = lk_get_frc_i(vfm_idle);
		dbg_i2("in:frc_i = %p\n", frc_i);
		if (!frc_i) {
			DBG_ERR("%s illegal %p\n", __func__, vfm_idle);
			return DPSS_ERR_UNDEFINED;
		}
		frc_i->in_vfm = vfm;
		if (pch->c.etype) { //ins path
			itf_get_status(pch, vfm, &frc_i->info_in);
		} else {
			frc_i->info_in.idx_m = pch->d->cnt_pre_get;
		}
	}
	dbg_h1("cvs:in vf 1<%d,%d> <%d,%d> addr<0x%lx,0x%lx>\n",
			vfm->canvas0_config[0].width,
			vfm->canvas0_config[0].height,
			vfm->width,
			vfm->height,
			vfm->canvas0_config[0].phy_addr,
			vfm->canvas0_config[1].phy_addr);
	dbg_h1("vfm:0x%x\n", vfm->canvas0Addr);
	if (VFM_IS_COMP_MODE(vfm->type)) {
		dbg_h1("cmp size:<%d,%d>, head_addr: 0x%lx, body_addr:0x%lx\n",
			vfm->compWidth, vfm->compHeight,
			vfm->compHeadAddr, vfm->compBodyAddr);
	}
	if (vfm->canvas0Addr != ((u32)-1)) {
		DBG_WARN("cvs id ???0x%x, 0x%x\n", vfm->canvas0Addr, ((u32)-1));
		cvs_id = vfm->canvas0Addr & 0xff;
		canvas_read(cvs_id, &cvs_in);
		vfm->canvas0_config[0].width = cvs_in.width;
		vfm->canvas0_config[0].height = cvs_in.height;
		vfm->canvas0_config[0].phy_addr = cvs_in.addr;
		vfm->canvas0_config[0].block_mode = cvs_in.blkmode;
		vfm->canvas0_config[0].endian = cvs_in.endian;
		cvs_id = (vfm->canvas0Addr >> 8) & 0xff;
		canvas_read(cvs_id, &cvs_in);
		vfm->canvas0_config[1].width = cvs_in.width;
		vfm->canvas0_config[1].height = cvs_in.height;
		vfm->canvas0_config[1].phy_addr = cvs_in.addr;
	}
	dbg_h1("cvs:in vf 2<%d,%d> <%d,%d> addr<0x%lx,0x%lx>\n",
			vfm->canvas0_config[0].width,
			vfm->canvas0_config[0].height,
			vfm->width,
			vfm->height,
			vfm->canvas0_config[0].phy_addr,
			vfm->canvas0_config[1].phy_addr);
	dbg_ins1("%s:%p,type=0x%x,%d\n", __func__,
		vfm, vfm->type, pch->d->cnt_in);
	ret = dpss_q_in_vfm(&pch->d->q_ch[EDPSS_Q_CH_IN], vfm_idle);
	if (ret)
		pch->d->cnt_in++;

	return DPSS_ERR_NONE;
}

enum DPSS_ERRORTYPE _cfg_buffer(struct dpss_ch_s *pch, struct vframe_s *vfm)
{
	unsigned char idx = 0;
	struct dpss_fifo_s *q_idle, *q_o;
	int ret_fifo;
	enum DPSS_ERRORTYPE ret_1 = DPSS_ERR_NONE;
	int err_num = 0;

	if (!pch || !pch->d) {
		//DBG_ERR("%s 01\n", __func__);
		err_num = 1;
		ret_1 = DPSS_ERR_INDEX_NOT_ACTIVE;
		goto cfg_buf_err;
	}
	if (!pch->d->di_front) {
		//DBG_ERR("%s: not di_front\n", __func__);
		err_num = 2;
		ret_1 = DPSS_ERR_UNSUPPORT;
		goto cfg_buf_err;
	}
	q_idle = &pch->d->q_ch[EDPSS_Q_CH_FRONT_IDLE];
	q_o = &pch->d->q_ch[EDPSS_Q_CH_FRONT_O];

	if (kfifo_is_empty(&q_idle->f) || kfifo_is_full(&q_o->f)) {
		//DBG_WARN("%s: full\n", __func__);
		err_num = 3;
		ret_1 = DPSS_ERR_IN_NO_SPACE;
		goto cfg_buf_err;
	}
	ret_fifo = kfifo_get(&q_idle->f, &idx);
	if (ret_fifo != 1 || idx >= DPSS_CFG_NUM) {
		//DBG_ERR("%s: full2 %d\n", __func__, idx);
		err_num = 4;
		ret_1 = DPSS_ERR_IN_NO_SPACE;
		goto cfg_buf_err;
	}

	dbg_ins1("%s:%px:%d\n", "cfg_buf", vfm, pch->d->cfg_cnt);
	pch->d->cfg_vfm[idx] = vfm;
	pch->d->cfg_cnt++;

	ret_fifo = kfifo_put(&q_o->f, idx);
	if (ret_fifo != 1) {
		//DBG_ERR("%s: full 3 %d\n", __func__, idx);
		err_num = 5;
		ret_1 = DPSS_ERR_IN_NO_SPACE;
		goto cfg_buf_err;
	}

	return DPSS_ERR_NONE;

cfg_buf_err:
	DBG_ERR("cfg_buf:%d\n", err_num);
	return ret_1;
}

enum DPSS_ERRORTYPE _back_buffer(struct dpss_ch_s *pch, struct vframe_s *vfm)
{
	bool ret = false;
	struct dpss_nr_i_s *nr_i;
	struct dpss_frc_i_s *frc_i;
	unsigned char type;

	if (!pch || !vfm) {
		DBG_ERR("%s: NULL param.\n", __func__);
		return DPSS_ERR_UNDEFINED;
	}
	if (!vfm->dpss_data || !pch->d) {
		DBG_ERR("%s:not dpss buffer, do nothing ch[%d] 0x%px\n",
			__func__, pch->c.ch, vfm);
		return DPSS_ERR_UNDEFINED;
	}

	if (kfifo_is_full(&pch->d->q_ch[EDPSS_Q_CH_BACK].f)) {
		DBG_WARN("%s:ch[%d] is full\n", __func__, pch->c.ch);
		return DPSS_ERR_IN_NO_SPACE;
	}
	type = lk_owner_get(vfm);
	if (type == DPSS_VF_OWNER_NR) {
		nr_i = lk_get_nr_i(vfm);
		if (!nr_i) {
			DBG_ERR("%s illegal? %p\n", __func__, vfm);
			return DPSS_ERR_UNDEFINED;
		}
		ret = dpss_q_in_vfm(&pch->d->q_ch[EDPSS_Q_CH_BACK], vfm);
		if (!ret) {
			DBG_ERR("%s:put err:ch[%d]\n", __func__, pch->c.ch);
			return DPSS_ERR_UNSUPPORT;
		}
		pch->d->cnt_pst_put++;
	} else if (type == DPSS_VF_OWNER_FRC) {
		frc_i = lk_get_frc_i(vfm);
		if (!frc_i) {
			DBG_ERR("%s illegal? %p\n", __func__, vfm);
			return DPSS_ERR_UNDEFINED;
		}
		dbg_h2("%s back buffer id : %d\n", __func__, frc_i->inp_idx);
		ret = dpss_q_in_vfm(&pch->d->q_ch[EDPSS_Q_CH_BACK], vfm);
		if (!ret) {
			DBG_ERR("%s:put err:ch[%d]\n", __func__, pch->c.ch);
			return DPSS_ERR_UNSUPPORT;
		}
		pch->d->cnt_pst_put++;
	} else {
		DBG_WARN("%s:to-do %d\n", __func__, type);
	}

	return DPSS_ERR_NONE;
}

//get ready or peek:
//static
struct vframe_s *_get_ready_block(int ch_index)
{
	//block:
	struct dpss_ch_s *pch;
	unsigned int ch;
	struct vframe_s *vfm = NULL;

	ch = (unsigned int)ch_index;
	if (ch > DPSS_CHANNEL_NUB)
		return NULL;
	pch = dpss_get_ch(ch);
	mutex_lock(&pch->c.ch_lock); //lock
	if (!pch->d || !pch->c.reg_s1) {
		DBG_ERR("%s:unreg?\n", __func__);
		mutex_unlock(&pch->c.ch_lock);	//unlock
		return NULL;
	}
	vfm = dpss_q_out_vfm(pch, EDPSS_Q_CH_RD);
	if (vfm)
		pch->d->cnt_pst_get++;
	mutex_unlock(&pch->c.ch_lock);	//unlock
	if (vfm)
		return vfm;
	return NULL;
}

static struct vframe_s *_get_ready(int ch_index)
{
	//block:
	struct dpss_ch_s *pch;
	unsigned int ch;
	struct vframe_s *vfm = NULL;

	ch = (unsigned int)ch_index;
	if (ch > DPSS_CHANNEL_NUB)
		return NULL;
	pch = dpss_get_ch(ch);

	if (!pch->d || !pch->c.reg_s1) {
		DBG_ERR("%s:unreg?\n", __func__);
		return NULL;
	}

	if (pch->d->vfm_dis_en && pch->d->vfm_dis) { //dbg only
		dbg_h1("%s:dbg:\n", __func__);
		pch->d->cnt_pst_get++;
		vfm = pch->d->vfm_dis;
		pch->d->vfm_dis_en = false;
		pch->d->vfm_dis = NULL;
		if (pch->c.reg_s2)
			vfm->type_ext |= VIDTYPE_EXT_FRC_LINK;
		return vfm;
	}
	vfm = dpss_q_out_vfm(pch, EDPSS_Q_CH_RD);
	if (vfm) {
		if (pch->c.reg_s2)
			vfm->type_ext |= VIDTYPE_EXT_FRC_LINK;
		pch->d->cnt_pst_get++;
		pch->c.disp_cnt++;
		dbg_h1("%s:cvs:out vf 1<%d,%d> <%d,%d> addr<0x%lx,0x%lx>\n",
			__func__,
			vfm->canvas0_config[0].width,
			vfm->canvas0_config[0].height,
			vfm->width,
			vfm->height,
			vfm->canvas0_config[0].phy_addr,
			vfm->canvas0_config[1].phy_addr);
		dbg_h1("vfm:0x%x typ:0x%x\n", vfm->canvas0Addr, vfm->type);
	}

	if (vfm)
		return vfm;
	return NULL;
}

//static
struct vframe_s *_peek_ready_block(int ch_index)
{
	//block:
	struct dpss_ch_s *pch;
	unsigned int ch;
	struct vframe_s *vfm = NULL;

	ch = (unsigned int)ch_index;
	if (ch > DPSS_CHANNEL_NUB)
		return NULL;
	pch = dpss_get_ch(ch);
	mutex_lock(&pch->c.ch_lock); //lock
	if (!pch->d || !pch->c.reg_s1) {
		DBG_ERR("%s:unreg?\n", __func__);
		mutex_unlock(&pch->c.ch_lock);	//unlock
		return NULL;
	}
	vfm = dpss_q_peek_vfm(pch, EDPSS_Q_CH_RD);
	mutex_unlock(&pch->c.ch_lock);	//unlock
	if (vfm)
		return vfm;
	return NULL;
}

static struct vframe_s *_peek_ready(int ch_index)
{
	//block:
	struct dpss_ch_s *pch;
	unsigned int ch;
	struct vframe_s *vfm = NULL;

	ch = (unsigned int)ch_index;
	if (ch > DPSS_CHANNEL_NUB)
		return NULL;
	pch = dpss_get_ch(ch);

	if (!pch->d || !pch->c.reg_s1) {
		DBG_ERR("%s:unreg?\n", __func__);

		return NULL;
	}
	if (pch->d->vfm_dis_en && pch->d->vfm_dis) { //dbg only
		vfm = pch->d->vfm_dis;
		return vfm;
	}
	vfm = dpss_q_peek_vfm(pch, EDPSS_Q_CH_RD);

	if (vfm)
		return vfm;
	return NULL;
}

enum E_DPSS_BLK_CMD {
	E_DPSS_BLK_CMD_NONE,
	E_DPSS_BLK_CMD_CREATE_FIX,
	E_DPSS_BLK_CMD_CREATE,
	E_DPSS_BLK_CMD_DESTROY,
	E_DPSS_BLK_CMD_IN_BUF,	//input buffer
	E_DPSS_BLK_CMD_IN_OUT_BUF,	//send display buffer to dpss for wr
	E_DPSS_BLK_CMD_BACK_BUF,	//send back dpss buffer
};

enum DPSS_ERRORTYPE dpss_block_task(unsigned int cmd, int ch_index, void *para)
{
	int ret = DPSS_ERR_NONE;
	struct dpss_ch_s *pch;
	unsigned int ch;
	struct dpss_init_parm *in_parm = (struct dpss_init_parm *)para;
	struct dpss_init_parm l_parm;

	if (!in_parm)
		DBG_INF("%s:%d frc only case\n", __func__, cmd);

	if (cmd == E_DPSS_BLK_CMD_CREATE_FIX) {
		//if (in_parm->dps_work_mode == DPSS_WORK_MODE_FRC)
		memcpy(&l_parm, &vfm_frc_path_parm, sizeof(l_parm));
		//else
		//	memcpy(&l_parm, &vfm_path_parm, sizeof(l_parm));
		ret = _create_instance(true, ch_index, &l_parm);
		return ret;
	}
	if (cmd == E_DPSS_BLK_CMD_CREATE) {
		ret = _create_instance(false, 0, para);
		return ret;
	}
	/*****************************************/
	ch = (unsigned int)ch_index;
	if (ch > DPSS_CHANNEL_NUB)
		return DPSS_ERR_INDEX_OVERFLOW;
	pch = dpss_get_ch(ch);

	if (cmd == E_DPSS_BLK_CMD_DESTROY)
		atomic_set(&pch->c.unreg_proc, 1);

	mutex_lock(&pch->c.ch_lock); //lock

	switch (cmd) {
	case E_DPSS_BLK_CMD_DESTROY:
		ret = _destroy_instance(pch);
		break;
	case E_DPSS_BLK_CMD_IN_BUF:
		ret = _input_buffer(pch, para);
		break;
	case E_DPSS_BLK_CMD_IN_OUT_BUF:
		ret = _cfg_buffer(pch, para);
		break;
	case E_DPSS_BLK_CMD_BACK_BUF:
		ret = _back_buffer(pch, para);
		break;
	default:
		DBG_ERR("%s:%d\n", __func__, cmd);
	break;
	}
	mutex_unlock(&pch->c.ch_lock);	//unlock

	if (cmd == E_DPSS_BLK_CMD_DESTROY)
		atomic_set(&pch->c.unreg_proc, 0);

	return ret;
}

/**************************************************************************************/

//dev_vfram_t -> dpss_dev_vfram_t
static struct dpss_dev_vfram_t *get_dev_vframe(unsigned int ch)
{
	if (ch < DPSS_CHANNEL_NUB)
		return &dpss_get_ch(ch)->c.dvfm;

	DBG_ERR("err:%s ch overflow %d\n", __func__, ch);
	return &dpss_get_ch(0)->c.dvfm;
}

static const char * const dpss_rev_name[4] = {
	"dpss",
	"dimulti.5",
	"dpss.1",
	"dpss.2",
};

static int _vf_notify_receiver(unsigned int ch, int event_type, void *data)
{
	return vf_notify_receiver(dpss_rev_name[ch], event_type, data);
}

static int _vf_notify_provider(unsigned int ch, int event_type, void *data)
{
	return vf_notify_provider(dpss_rev_name[ch], event_type, data);
}

static void _vf_put(struct vframe_s *vf, unsigned int ch)
{
	//sum_p_inc(ch);
	vf_put(vf, dpss_rev_name[ch]);
}

static struct vframe_s *_vf_peek(unsigned int ch)
{
	return vf_peek(dpss_rev_name[ch]);
}

void dbg_vfm_peek(unsigned int ch)
{
	struct vframe_s *vfm;

	vfm = vf_peek(dpss_rev_name[ch]);
	if (!vfm) {
		dbg_h1("ch[%d]:%s:no vfm\n", ch,
			dpss_rev_name[ch]);
	}
}

static struct vframe_s *_vf_get(unsigned int ch)
{
	//sum_g_inc(ch);
	struct vframe_s *vf = vf_get(dpss_rev_name[ch]);

	if (vf)
		dbg_s2("%s vf:0x%px, index:%d, pts_us64:0x%llx, video_id:%d\n",
			__func__, vf,
			vf->index, vf->pts_us64, vf->vf_ud_param.ud_param.instance_id);
	return vf;
}

static void _vfm_polling_get_pre_block(struct dpss_ch_s *pch, int inp_is_full)
{
	struct vframe_s *vfm = NULL, *vfm2;
	unsigned int ch;
	int ret_in;
	bool lock_ret;

	if (!dpss_dbg_is_run())
		return;

	//mutex_lock(&pch->c.ch_lock); //lock
	lock_ret = mutex_trylock(&pch->c.ch_lock);
	if (!lock_ret) {
		DBG_WARN("pre_block:wait\n");
		return;
	}
	ch = pch->c.ch;
	if (inp_is_full) {
		vfm = _vf_get(ch);
		if (vfm) {
			_vf_put(vfm, ch);
			_vf_notify_provider(ch, VFRAME_EVENT_RECEIVER_PUT, NULL);
		}
		DBG_WARN("%s:inp is full\n", __func__);
		mutex_unlock(&pch->c.ch_lock);	//unlock
		return;
	}
	vfm = _vf_peek(ch);
	if (!vfm) {
		DBG_WARN("%s:peek vfm is null\n", __func__);
		mutex_unlock(&pch->c.ch_lock);	//unlock
		return;
	}
	ret_in = _input_buffer(pch, vfm);
	if (ret_in == DPSS_ERR_NONE) {
		pch->d->cnt_pre_get++;

		vfm2 = _vf_get(ch);
		if (vfm != vfm2) {
			DBG_WARN("%s:ch[%d] peek:0x%px, get:0x%px\n",
				__func__, ch, vfm, vfm2);
		}
		dbg_i2("pre get:0x%px\n", vfm2);
	} else {
		DBG_WARN("%s:ch[%d], err:%d\n", __func__, ch, ret_in);
	}
	mutex_unlock(&pch->c.ch_lock);	//unlock
}

static void itf_put_back_input(struct dpss_ch_s *pch, struct vframe_s *vfm)
{
	unsigned int ch;

	ch = pch->c.ch;
	if (pch->c.etype) {/* ins*/
		dbg_ins1("%s:empty_in:%p:%d\n", __func__, vfm, pch->d->cnt_pre_put);
		pch->c.parm.ops.empty_input_done(pch->c.parm.ops.arg, vfm);
	} else {	/* vframe path */
		_vf_put(vfm, ch);
		_vf_notify_provider(ch,
			      VFRAME_EVENT_RECEIVER_PUT, NULL);
	}
	pch->d->cnt_pre_put++;
}

static void _vfm_polling_put_pre_block(struct dpss_ch_s *pch)
{
	struct vframe_s *vfm = NULL; // *vfm_in = NULL;
	unsigned int len; // ch,
	bool lock_ret;

	int i;

	//mutex_lock(&pch->c.ch_lock); //lock
	lock_ret = mutex_trylock(&pch->c.ch_lock);
	if (!lock_ret) {
		DBG_WARN("put pre:wait\n");
		return;
	}
	//	ch = pch->c.ch;
	//to-do list
	//recycle to pre:
	// len = kfifo_len(&pch->d->q_ch[EDPSS_Q_CH_IN_RECYCLE].f); // diff
	len = kfifo_len(&pch->d->q_ch[EDPSS_Q_CH_V_RECYCLE].f);
	dbg_h2("recycle len = %d\n", len);
	if (len) {
		for (i = 0; i < len; i++) {
			vfm = dpss_in_rck_o(pch);
			if (!vfm) {
				DBG_ERR("%s:%d,null\n", __func__, i);
				break;
			}
			/***********************/
			itf_put_back_input(pch, vfm);
		}
	}
	mutex_unlock(&pch->c.ch_lock);	//unlock
}

static void _vfm_polling_put_pre_block_frc(struct dpss_ch_s *pch)
{
	struct vframe_s *vfm = NULL, *vfm_in = NULL;
	unsigned int ch, len;
	struct dpss_frc_i_s *frc_i;
	int i;

	mutex_lock(&pch->c.ch_lock); //lock
	ch = pch->c.ch;
	//recycle to pre:
	len = kfifo_len(&pch->d->q_ch[EDPSS_Q_CH_IN_RECYCLE].f);
	dbg_h2("recycle len = %d\n", len);
	if (len) {
		for (i = 0; i < len; i++) {
			vfm = dpss_q_out_vfm(pch, EDPSS_Q_CH_IN_RECYCLE);
			if (!vfm || !vfm->dpss_data) {
				DBG_WARN("%s:ch[%d] re no vfm? i=%d,len=%d,\n",
					__func__, pch->c.ch, i, len);
				break;
			}
			frc_i = lk_get_frc_i(vfm);
			vfm_in = frc_i->in_vfm;
			/*clear vf_s1:*/
			frc_i->in_vfm = NULL;
			memset(&frc_i->sub_vf_in, 0, sizeof(frc_i->sub_vf_in));
			//to-do : if clean vf_s2 frc
			dpss_q_in_vfm(&pch->d->q_ch[EDPSS_Q_CH_IDLE_FRC], vfm);
			itf_put_back_input(pch, vfm_in);
		}
	}
	mutex_unlock(&pch->c.ch_lock);	//unlock
}

void _ins_polling_send_ready(struct dpss_ch_s *pch)
{
	struct vframe_s *vfm = NULL;
	int i;
	unsigned int len;
	bool lock_ret = false;

	//int ret_in;

	//mutex_lock(&pch->c.ch_lock); //lock
	for (i = 0; i < 3; i++) {
		lock_ret = mutex_trylock(&pch->c.ch_lock);
		if (lock_ret)
			break;
		usleep_range(100, 200);
	}

	if (!lock_ret) {
		dbg_ct0("send ready:wait\n");
		return;
	}
	//ready to post:
	len = kfifo_len(&pch->d->q_ch[EDPSS_Q_CH_RD].f);
	if (len) {
		for (i = 0; i < len; i++) {
			vfm = dpss_q_out_vfm(pch, EDPSS_Q_CH_RD);
			if (!vfm) {
				DBG_WARN("%s:ch[%d]no vfm? i=%d,len=%d,\n",
					__func__, pch->c.ch, i, len);
				break;
			}
			pch->d->cnt_pst_get++;
			pch->c.disp_cnt++;
			dbg_h2("send display in_cnt= %d disp_cnt = %d\n",
				pch->c.in_cnt, pch->c.disp_cnt);
			pch->c.parm.ops.fill_output_done(pch->c.parm.ops.arg, vfm);
		}
	}

	mutex_unlock(&pch->c.ch_lock);	//unlock
}

void itf_polling_in_o(struct dpss_ch_s *pch)
{
	int inp_is_full = 0;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();

	if (!pch) {
		DBG_ERR("%s: NULL param.\n", __func__);
		return;
	}

	if (!pch->d || !pch->c.reg_s1)	{
		DBG_WARN("%s:ch[%d]not reg?\n", __func__, pch->c.ch);
		return;
	}

	if (!pch->c.reg_s2 || atomic_read(&pch->c.unreg_proc)) {
		dbg_i0("%s:ch[%d]:not reg 2:\n", __func__, pch->c.ch);
		return;
	}

	if (pch->c.parm.dps_work_mode == DPSS_WORK_MODE_FRC) {
		if (!pchip_st) {
			dbg_i0("%s: frc st is null\n", __func__);
			return;
		}
		_vfm_polling_put_pre_block_frc(pch);
		if (!src0_data_trigger() && !pchip_st->state_st.is_first_frame) {
			inp_is_full = 1;
			DBG_WARN("%s:src0 no rd (%d)\n", __func__, inp_is_full);
		}
	} else {
		_vfm_polling_put_pre_block(pch);
	}

	if (pch->c.etype)  //ins
		_ins_polling_send_ready(pch);
	else
		_vfm_polling_get_pre_block(pch, inp_is_full);
}

//when put ready buffer, call this function.
void itf_vfm_path_ready_notify(struct dpss_ch_s *pch)
{
	_vf_notify_receiver(pch->c.ch,
			      VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
}

static void dev_vframe_reg(struct dpss_dev_vfram_t *pvfm)
{
	vf_reg_provider(&pvfm->di_vf_prov);
	vf_notify_receiver(pvfm->name, VFRAME_EVENT_PROVIDER_START, NULL);
}

static int _ori_event_qurey_vdin2nr(unsigned int channel)
{
	return 0;
}

static int _ori_event_reset(unsigned int channel)
{
	return 0;
}

static int _ori_event_light_unreg(unsigned int channel)
{
	return 0;
}

static int _ori_event_light_unreg_revframe(unsigned int channel)
{
	DBG_INF("%s:VFRAME_EVENT_PROVIDER_LIGHT_UNREG_RETURN_VFRAME\n",
		__func__);

	return 0;
}

static int _irq_ori_event_ready(unsigned int channel)
{
	return 0;
}

static int _ori_event_ready(unsigned int channel)
{
	dpss_tsk_m_wake(9);
	_irq_ori_event_ready(channel);
	return 0;
}

static int _vf_l_states(struct vframe_states *states, unsigned int channel)
{
//#if 0	//to-do
//	struct div2_mm_s *mm = dim_mm_get(channel);
//	struct dim_sum_s *psumx = get_sumx(channel);
//#endif
	/*pr_info("%s: ch[%d]\n", __func__, channel);*/
	if (!states)
		return -1;
//#if 0	//to-do
//	states->vf_pool_size = mm->sts.num_local;
//	states->buf_free_num = psumx->b_pre_free;
//
//	states->buf_avail_num = psumx->b_pst_ready;
//	states->buf_recycle_num = psumx->b_recyc;
//	if (dimp_get(edi_mp_di_dbg_mask) & 0x1) {
//		di_pr_info("di-pre-ready-num:%d\n", psumx->b_pre_ready);
//		di_pr_info("di-display-num:%d\n", psumx->b_display);
//		di_pr_info("di-pst-link-num:%d\n", psumx->b_pst_link);
//	}
//#endif
	return 0;
}

static int _ori_event_qurey_state(unsigned int channel)
{
	/*int in_buf_num = 0;*/
	struct vframe_states states;
//#if 0
//	if (dim_vcry_get_flg())
//		return RECEIVER_INACTIVE;
//
//	/*fix for ucode reset method be break by di.20151230*/
//	_vf_l_states(&states, channel);
//	if (states.buf_avail_num > 0)
//		return RECEIVER_ACTIVE;
//
//	if (_vf_notify_receiver(channel,
//				  VFRAME_EVENT_PROVIDER_QUREY_STATE,
//				  NULL) == RECEIVER_ACTIVE)
//		return RECEIVER_ACTIVE;
//
//	return RECEIVER_INACTIVE;
//#else
	//to-do
	_vf_l_states(&states, channel);
	return RECEIVER_ACTIVE;
//#endif
}

static void  _ori_event_set_3D(int type, void *data, unsigned int channel)
{
}

static void  _ori_event_set_fcc(unsigned int ch)
{
	DBG_INF("%s: ch[%d]\n", __func__, ch);
}

/*--------------------------*/

static const char * const dpss_receiver_event_cmd[] = {
	"",
	"_UNREG",
	"_LIGHT_UNREG",
	"_START",
	NULL,	/* "_VFRAME_READY", */
	NULL,	/* "_QUREY_STATE", */
	"_RESET",
	NULL,	/* "_FORCE_BLACKOUT", */
	"_REG",
	"_LIGHT_UNREG_RETURN_VFRAME",
	NULL,	/* "_DPBUF_CONFIG", */
	NULL,	/* "_QUREY_VDIN2NR", */
	NULL,	/* "_SET_3D_VFRAME_INTERLEAVE", */
	NULL,	/* "_FR_HINT", */
	NULL,	/* "_FR_END_HINT", */
	NULL,	/* "_QUREY_DISPLAY_INFO", */
	NULL,	/* "_PROPERTY_CHANGED", */
};

#define VFRAME_EVENT_PROVIDER_CMD_MAX	16

static int dpss_receiver_event_fun(int type, void *data, void *arg)
{
	struct dpss_dev_vfram_t *pvfm;
	unsigned int ch;
	unsigned int ch_i;
	int ret = 0;
	struct dpss_ch_s *pch;
//	char *provider_name = (char *)data;

	ch_i = *(int *)arg;
	ch = (unsigned int)ch_i;
	pch = dpss_get_ch(ch);
	pvfm = get_dev_vframe(ch);

	if (type <= VFRAME_EVENT_PROVIDER_CMD_MAX	&&
	    dpss_receiver_event_cmd[type]) {
		dbg_i0("ch[%d]:%s:%d:%s\n", ch, "event",
		       type,
		       dpss_receiver_event_cmd[type]);
	}

	switch (type) {
	case VFRAME_EVENT_PROVIDER_UNREG:
		vf_unreg_provider(&pch->c.dvfm.di_vf_prov);
		dpss_block_task(E_DPSS_BLK_CMD_DESTROY, ch_i, NULL);
		break;
	case VFRAME_EVENT_PROVIDER_REG:
		dpss_block_task(E_DPSS_BLK_CMD_CREATE_FIX, ch_i, NULL);

		dev_vframe_reg(&pch->c.dvfm);
		break;
	case VFRAME_EVENT_PROVIDER_START:
		break;

	case VFRAME_EVENT_PROVIDER_LIGHT_UNREG:
		ret = _ori_event_light_unreg(ch);
		break;
	case VFRAME_EVENT_PROVIDER_VFRAME_READY:
		ret = _ori_event_ready(ch);
		break;
	case VFRAME_EVENT_PROVIDER_QUREY_STATE:
		ret = _ori_event_qurey_state(ch);
		break;
	case VFRAME_EVENT_PROVIDER_RESET:
		ret = _ori_event_reset(ch);
		break;
	case VFRAME_EVENT_PROVIDER_LIGHT_UNREG_RETURN_VFRAME:
		ret = _ori_event_light_unreg_revframe(ch);
		break;
	case VFRAME_EVENT_PROVIDER_QUREY_VDIN2NR:
		ret = _ori_event_qurey_vdin2nr(ch);
		break;
	case VFRAME_EVENT_PROVIDER_SET_3D_VFRAME_INTERLEAVE:
		_ori_event_set_3D(type, data, ch);
		break;
	case VFRAME_EVENT_PROVIDER_FR_HINT:
	case VFRAME_EVENT_PROVIDER_FR_END_HINT:
		vf_notify_receiver(pvfm->name, type, data);
		break;
	case VFRAME_EVENT_PROVIDER_FCC:
		_ori_event_set_fcc(ch);
		break;

	default:
		break;
	}

	return ret;
}

static const struct vframe_receiver_op_s dpss_vf_receiver = {
	.event_cb	= dpss_receiver_event_fun
};

static struct vframe_s *dpss_vf_peek(void *arg)
{
	unsigned int ch = *(int *)arg;
	struct vframe_s *vfm;

	vfm = _peek_ready(ch);//_peek_ready_block(ch);
	return vfm;
}

static struct vframe_s *dpss_vf_get(void *arg)
{
	int ch = *(int *)arg;
	struct vframe_s *vfm;

	vfm = _get_ready(ch); //_get_ready_block(ch);

	return vfm;
}

static void dpss_vf_put(struct vframe_s *vf, void *arg)
{
	int ch_i = *(int *)arg;
	unsigned int ch;
	struct dpss_ch_s *pch;

	ch = (unsigned int)ch_i;
	pch = dpss_get_ch(ch);

	//dpss_block_task(E_DPSS_BLK_CMD_BACK_BUF, ch, vf);
	_back_buffer(pch, vf);
}

static int dpss_event_cb(int type, void *data, void *private_data)
{
	if (type == VFRAME_EVENT_RECEIVER_FORCE_UNREG) {
		DBG_WARN("%s: RECEIVER_FORCE_UNREG return\n",
			__func__);
		return 0;
	}
	return 0;
}

static int dpss_vf_states(struct vframe_states *states, void *arg)
{
	unsigned int ch = *(int *)arg;
//	struct dpss_ch_s *pch;

	if (!states)
		return -1;
//	pch = dpss_get_ch(ch);

	dbg_i2("%s:ch[%d]\n", __func__, ch);

	_vf_l_states(states, ch);
	return 0;
}

static const struct vframe_operations_s dpss_vf_provider = {
	.peek		= dpss_vf_peek,
	.get		= dpss_vf_get,
	.put		= dpss_vf_put,
	.event_cb	= dpss_event_cb,
	.vf_states	= dpss_vf_states,
};

void itf_vfm_path_exit(void)
{
	struct dpss_dev_vfram_t *pvfm;
	int ch;
	struct dpss_ch_s *pch;

	for (ch = 0; ch < DPSS_CHANNEL_NUB; ch++) {
		pch = dpss_get_ch(ch);
		pvfm = &pch->c.dvfm;
		vf_unreg_provider(&pvfm->di_vf_prov);
		vf_unreg_receiver(&pvfm->di_vf_recv);
	}
	dbg_i0("%s finish\n", __func__);
}

//vframe path: prob
void itf_vfm_path_init(void)
{
	struct dpss_dev_vfram_t *pvfm;
	int ch;
	struct dpss_ch_s *pch;

	for (ch = 0; ch < DPSS_CHANNEL_NUB; ch++) {
		dbg_i2("%s:ch[%d]\n", __func__, ch);
		pch = dpss_get_ch(ch);
		//pch->itf.ch = ch; //2020-12-21
		pvfm = &pch->c.dvfm;
		pvfm->name = dpss_rev_name[ch];
		pvfm->index = ch;
		pch->c.ch = ch;
		/*receiver:*/
		vf_receiver_init(&pvfm->di_vf_recv, pvfm->name,
				 &dpss_vf_receiver, &pvfm->index);
		vf_reg_receiver(&pvfm->di_vf_recv);

		/*provider:*/
		vf_provider_init(&pvfm->di_vf_prov, pvfm->name,
				 &dpss_vf_provider, &pvfm->index);
	}
}

//#endif //vfm path

int dpss_create_instance(struct dpss_init_parm *parm)
{
	int ret;

	ret = dpss_block_task(E_DPSS_BLK_CMD_CREATE, 0, parm);

	return ret;
}
EXPORT_SYMBOL(dpss_create_instance);

int dpss_destroy_instance(int index)
{
	int ret;

	ret = dpss_block_task(E_DPSS_BLK_CMD_DESTROY, index, NULL);
	return ret;
}
EXPORT_SYMBOL(dpss_destroy_instance);

enum DPSS_ERRORTYPE dpss_empty_input_buffer(int index, struct vframe_s *vfm)
{
	int ret;

	ret = dpss_block_task(E_DPSS_BLK_CMD_IN_BUF, index, vfm);
	dpss_tsk_m_wake(21);

	return ret;
}
EXPORT_SYMBOL(dpss_empty_input_buffer);

enum DPSS_ERRORTYPE dpss_fill_output_buffer(int index, struct vframe_s *vfm)
{
	int ret;

	ret = dpss_block_task(E_DPSS_BLK_CMD_BACK_BUF, index, vfm);

	return ret;
}
EXPORT_SYMBOL(dpss_fill_output_buffer);

enum DPSS_ERRORTYPE dpss_empty_out_buffer(int index, struct vframe_s *vfm)
{
	int ret;

	ret = dpss_block_task(E_DPSS_BLK_CMD_IN_OUT_BUF, index, vfm);

	return ret;
}
EXPORT_SYMBOL(dpss_empty_out_buffer);

int dpss_get_state(int index, enum DPSS_STATE cmd_state, struct vframe_s *vfm,
	struct dpss_status *state)
{
	int ret = DPSS_ERR_NONE;
	struct dpss_ch_s *pch;
	unsigned int ch;

	ch = (unsigned int)index;
	if (ch > DPSS_CHANNEL_NUB)
		return DPSS_ERR_INDEX_OVERFLOW;

	pch = dpss_get_ch(ch);
	state->buf_status = pch->c.buf_status;

	dpss_get_game_lock_st(state);

	return ret;
}
EXPORT_SYMBOL(dpss_get_state);

int dpss_get_vf_info(struct vframe_s *vf, struct dpss_out_vf_info *dpss_out_info)
{
	int ret = DPSS_ERR_NONE;
	struct dpss_nr_i_s *nr_i;
	unsigned char type;

	type = lk_owner_get(vf);
	if (type == DPSS_VF_OWNER_OTHER) {
		//bypass mode:
		dpss_out_info->flag = 0xff10; //bypass tmp;
		return DPSS_ERR_UNDEFINED;
	}
	nr_i = lk_get_nr_i(vf);
	if (!nr_i) {	//check  tmp
		dbg_h2("illegal 0x%px\n", vf);
		dpss_out_info->flag = 0xfffff;
		return DPSS_ERR_UNDEFINED;
	}
	memcpy(dpss_out_info, &nr_i->info_out, sizeof(*dpss_out_info));

	return ret;
}
EXPORT_SYMBOL(dpss_get_vf_info);

//
int dpss_cmd_asy(int index, unsigned int cmd_asy, struct dpss_cmd_a_s *para)
{
	struct dpss_ch_s *pch;
	unsigned int ch;

	ch = (unsigned int)index;

	if (ch > DPSS_CHANNEL_NUB)
		return DPSS_ERR_INDEX_OVERFLOW;
	pch = dpss_get_ch(ch);
	if (!para || !pch || !pch->d) {
		DBG_ERR("%s:no para:work\n", __func__);
		return DPSS_ERR_UNDEFINED;
	}

	if (cmd_asy == DPSS_CMD_ASY_WORKMODE) {
		//pch->c.parm.dps_work_mode = para->para;
		pch->d->new_w_mode = para->para;
		pch->d->new_w_mode |= DPSS_PARA_UPDATE;
		dbg_i0("%s:chg_work_mode:0x%x->0x%x\n",
			__func__,
			pch->c.parm.dps_work_mode,
			pch->d->new_w_mode);
	} else if (cmd_asy == DPSS_CMD_ASY_2_WORKMODE) {
		//pch->c.parm.dps_work_mode = para->para;
		pch->d->w_mode_2 = para->para;

		dbg_i0("%s:chg w_mode_2:0x%x\n",
			__func__,
			pch->d->w_mode_2);
	} else if (cmd_asy == DPSS_CMD_ENABLE_FRC) {
		dbg_i0("%s: frc_enable: %d\n", __func__, para->para);
		if (dpss_frc_get_enable() == 1)
			dpss_frc_set_mc_bypass((u8)para->para);
	}
	return DPSS_ERR_NONE;
}
EXPORT_SYMBOL(dpss_cmd_asy);

bool check_need_do_game_frc(struct dpss_init_parm *parm, struct vframe_s *vf)
{
	int i;
	u32 in_frm_rate, in_frm_frac, out_frm_rate;
	struct vinfo_s *vinfo;
	bool need_do_game_frc = true;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct dpss_game_state_s *game_st;
	struct dpss_init_parm *dpss_parm = parm;

	vinfo = get_current_vinfo();

	if (!pchip_st) {
		dbg_h0("pchip_st or vfs is null\n");
		return need_do_game_frc;
	}

	if (!vinfo)
		return need_do_game_frc;

	//INTERLACE
	if (vf->type & VIDTYPE_INTERLACE)
		need_do_game_frc = false;

	// game flag
	if (!(vf->flag & VFRAME_FLAG_GAME_MODE) ||
		!(vf->flag_ext & VDIN_GAME_FRC_FLAG_EN))
		need_do_game_frc = false;

	//Resolution
	for (i = 0; i < GAME_RES_LIST_MAX; i++)
		if (vf->compWidth == frc_res_list[i].width &&
			vf->compHeight == frc_res_list[i].height)
			break;

	if (i == GAME_RES_LIST_MAX)
		need_do_game_frc = false;

	//N2M
	in_frm_rate = 96000 / vf->duration;
	in_frm_frac = 96000 * 1000 / vf->duration % 1000;
	in_frm_rate = in_frm_rate + (in_frm_frac + 500) / 1000;
	out_frm_rate = vinfo->sync_duration_num / vinfo->sync_duration_den;
	for (i = 0; i < GAME_N2M_LIST_MAX; i++)
		if (in_frm_rate == frc_n2m_list[i].input_framerate &&
			out_frm_rate == frc_n2m_list[i].output_framerate)
			break;

	if (i == GAME_N2M_LIST_MAX)
		need_do_game_frc = false;

	game_st = &pchip_st->game_st;
	dpss_parm->frc_parm.do_game_frc = need_do_game_frc;
	dpss_parm->frc_parm.width = vf->compWidth;
	dpss_parm->frc_parm.height = vf->compHeight;
	dpss_parm->frc_parm.duration =  vf->duration;
	dpss_parm->frc_parm.in_frm_rate = in_frm_rate;
	dpss_parm->frc_parm.out_frm_rate = out_frm_rate;
	game_st->do_game_frc = need_do_game_frc;

	dbg_h2("game ret:%d, res:(%d,%d), in/out:(%d/%d), frac:%d\n",
		need_do_game_frc, vf->compWidth, vf->compHeight,
		in_frm_rate, out_frm_rate, in_frm_frac);

	return need_do_game_frc;
}

//dpss bypas
unsigned char dpss_is_bypass(struct dpss_init_parm *parm, struct vframe_s *vf)
{
	unsigned int reason = 0;

	/*need bypass*/
	if (!vf) {
		pr_err("dpss param is invalid.\n");
		return reason;
	}

	if (dpss_bypass_all) {
		reason = 0x1;
	} else if (vf->flag & VFRAME_FLAG_GAME_MODE) {
		//if (!check_need_do_game_frc(parm, vf)) {
		if (check_need_do_game_frc(parm, vf)) {
			//reason = 0x2;// game mode
			//dbg_h2("game mode, no need do dpss.\n");
			reason = 0x0;// normal dpss
			dbg_h2("frc game mode, need do dpss.\n");
		// } else if (parm->dps_work_mode == DPSS_WORK_MODE_FRC) {
		//	reason = 0x0;// normal dpss
		//	dbg_h2("frc game mode, need do dpss.\n");
		} else {
			reason = 0x2;// game mode
			dbg_h2("support frc game, cur is normal game, no need do dpss.\n");
		}
	} else if (vf->flag & VFRAME_FLAG_PC_MODE) {
		if (vf->type & VIDTYPE_INTERLACE && vf->type & VIDTYPE_VIU_422)
			reason = 0x0;
		else
			reason = 0x3;/*PC mode no need do dpss */
	} else if (vf->fence) {
		reason = 0x4;/*low latency mode no need do dpss */
	} else if (vf->type & VIDTYPE_INTERLACE &&
		((vf->type & VIDTYPE_COMPRESS) ?
			vf->compWidth : vf->width) == 3840) {
		reason = 0x5;/*4k interlace no need do dpss*/
	} else if (vf->vf_vrr_param.frc_get_vrr >= 1 &&
		vf->vf_vrr_param.frc_get_vrr <= 5) {
		reason = 0x6;/*vrr frame no need do dps*/
	} else if (vf->type_ext & VIDTYPE_EXT_LCEVC) {
		reason = 0x8;/*lcevc*/
	} else if ((vf->compWidth > 2560 || vf->width > 2560) &&
			(vf->bitdepth & BITDEPTH_Y10) &&
			(vf->type & VIDTYPE_VIU_444 ||
			vf->type & VIDTYPE_RGB_444) &&
			(vf->type & VIDTYPE_COMPRESS)) {
		reason = 0x9;/* above 2560*1440 444 10bit afbc no need do dpss*/
	} else if (((vf->type & VIDTYPE_COMPRESS) &&
			(vf->compWidth < DPSS_BYPASS_WIDTH ||
			vf->compHeight < DPSS_BYPASS_HEIGHT ||
			vf->width < DPSS_BYPASS_WIDTH ||
			vf->height < DPSS_BYPASS_HEIGHT)) ||
			(!(vf->type & VIDTYPE_COMPRESS) &&
			(vf->width < DPSS_BYPASS_WIDTH ||
			vf->height < DPSS_BYPASS_HEIGHT))) {
		reason = 0xa;/*input height less than 120, not do dpss*/
	} else if (VFM_IS_I_SRC(vf->type) &&
			vf->height > 1080) {
		reason = 0xb;/*> 1920*1080*/
	} else {
		reason = 0x0;
	}

	dbg_ins0("bypass: %d\n", reason);
	return reason;
}

void dpss_game_frc_lock(struct dpss_init_parm *parm)
{
	u32 in_frm_rate, out_frm_rate;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct dpss_game_state_s *game_st;

	if (!pchip_st)
		return;

	if (!parm->frc_parm.do_game_frc)
		return;

	game_st = &pchip_st->game_st;
	in_frm_rate = parm->frc_parm.in_frm_rate;
	out_frm_rate = parm->frc_parm.out_frm_rate;
	if (out_frm_rate / in_frm_rate > 1)
		game_st->game_n2m = 1;
	else
		game_st->game_film = 1;

	if (game_st->game_vrr_en) {
		venc_vrr_vdin();
		dbg_h0("do frc game mdoe:[film:%d, n2m:%d]\n",
			game_st->game_film, game_st->game_n2m);
	} else {
		game_st->game_n2m = 0;
		game_st->game_film = 0;
		venc_vrr_vdin();
		dbg_h0("frc only mdoe:[film:%d, n2m:%d]\n",
			game_st->game_film, game_st->game_n2m);
	}
}

/*****************************************
 * pre-allocate
 *****************************************/
int dpss_buf_ize = 4096;
module_param_named(dpss_buf_ize, dpss_buf_ize, int, 0664);

int dpss_yuv_buf_ize = 4096;
module_param_named(dpss_yuv_buf_ize, dpss_yuv_buf_ize, int, 0664);

int dpss_yuv_buf_nub = DPSS_HW_LOOP_IN_OUT_BUF_NUB;
module_param_named(dpss_yuv_buf_nub, dpss_yuv_buf_nub, int, 0664);

void dpss_pre_buf_prob(void)
{
	struct dpss_dd_s *dd;

	dd = dpss_get_dd();
	dpss_buf_ize = dd->uhd_sml_info.size_t_nr_frc_s;
	dpss_yuv_buf_ize = dd->fd_afrc_info.size_body;
	dbg_m1("%s:0x%x,0x%x\n", __func__, dpss_buf_ize, dpss_yuv_buf_ize);
}

void dpss_submit_pre_buf(int inst_id, int mem_flags)
{
	submit_prealloc_job(PREALLOC_DPSS_SML_TYPE, 1, dpss_buf_ize,
		PAGE_SHIFT, 0, -1);
	submit_prealloc_job(PREALLOC_DPSS_YUV_TYPE, dpss_yuv_buf_nub, dpss_yuv_buf_ize,
		PAGE_SHIFT, mem_flags, -1);
}
EXPORT_SYMBOL(dpss_submit_pre_buf);

void dpss_release_pre_buf(int inst_id)
{
	release_prealloc_job_with_type(-1, PREALLOC_DPSS_SML_TYPE);
	release_prealloc_job_with_type(-1, PREALLOC_DPSS_YUV_TYPE);
}
EXPORT_SYMBOL(dpss_release_pre_buf);

/**************************************************************************************/
