// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/module.h>

#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>

#include "../tvin_global.h"
#include "../tvin_frontend.h"
#include "../tvin_format_table.h"
#include "vdin_mem_scatter.h"

/* Amlogic headers */
#include <linux/amlogic/media/vfm/vframe.h>
#ifdef CONFIG_AMLOGIC_TEE
#include <linux/amlogic/tee.h>
#endif

/* Local headers */
#include "../tvin_format_table.h"
#include "vdin_afbce.h"
#include "vdin_drv.h"

#define SCT_PRINT_CTL_INIT	BIT(0) /* print sct memory init info */
#define SCT_PRINT_CTL_EXIT	BIT(1) /* print sct memory exit info */
#define SCT_PRINT_CTL_ALLOC_IDX	BIT(2)
#define SCT_PRINT_CTL_FREE_TAIL	BIT(3)
#define SCT_PRINT_CTL_FREE_IDX	BIT(4)
#define SCT_PRINT_CTL_QUE_WORK	BIT(5) /* print queue work */
#define SCT_PRINT_CTL_WORKER	BIT(6) /* print worker processing */
#define SCT_PRINT_CTL_MMU_NUM	BIT(7) /* print worker processing */
#define SCT_PRINT_CTL_GAME	BIT(8) /* print game processing */
#define SCT_PRINT_CTL_WARN	BIT(9) /* buffer warning info */
#define SCT_PRINT_CTL_MEM_HLD	BIT(10) /* for keeper,scatter mem handle */

#define MAX_ADDR_SHIFT PAGE_SHIFT
#define PAGE_ADDR(mm_page) (ulong)(((mm_page) >> MAX_ADDR_SHIFT)\
			<< MAX_ADDR_SHIFT)

static u64 cur_to_usecs(void)/*2019*/
{
	u64 cur = sched_clock();

	do_div(cur, NSEC_PER_USEC);
	return cur;
}

phy_addr_type vdin_sct_get_page_addr(struct codec_mm_scatter *mms, int id)
{
	phy_addr_type page;

	if (!(mms) || !(mms)->pages_list ||
	     (mms)->page_cnt <= (id) || (id) < 0)
		return 0;
	page = mms->pages_list[id];
	return PAGE_ADDR(page);
}

/* Free buffers of these vf which will put into write list */
void vdin_sct_free_wr_list_idx(struct vf_pool *p, struct vframe_s *vf)
{
	struct vf_entry *vfe = NULL;
	struct vdin_dev_s *devp = NULL;
	int err = 0;

	vfe = container_of(vf, struct vf_entry, vf);
	if (!vfe) {
		err = -1;
		goto error;
	}

	if (vfe->sct_stat != VFRAME_SCT_STATE_INIT) {
		devp = (struct vdin_dev_s *)p->priv;
		if (!devp->afbce_info) {
			err = -2;
			goto error;
		}
		//we are not free tail in game mode, so we need to free idx here
		//otherwise it'll hold too much memory
		if (devp->debug.dbg_sct_ctl & DBG_SCT_CTL_NO_FREE_WR_LIST) {
			vdin_sct_free(p, vf->index);
			vfe->sct_stat = VFRAME_SCT_STATE_INIT;
		}
	}
	return;
error:
	pr_err("vdin sct free error:%d", err);
	return;
}

void vdin_sct_read_mmu_num(struct vdin_dev_s *devp, struct vf_entry *vfe)
{
	unsigned int *paddr = NULL;
	unsigned int idx;

	if (devp->mem_type == VDIN_MEM_TYPE_SCT && vfe) {
		if (devp->dtdata->hw_ver == VDIN_HW_T6W || devp->dtdata->hw_ver == VDIN_HW_T6X) {
			paddr = rdma_get_read_back_addr(devp->rdma_read_handle, &devp->reg_hnd);
			if (paddr) {
				vfe->vf.afbce_num = *paddr;
			} else {
				vfe->vf.afbce_num = 0;
			}
			devp->afbce_mmu_num = 0;
		} else if (devp->dtdata->hw_ver == VDIN_HW_T3X) {
			vfe->vf.afbce_num = rd(devp->addr_offset, VDIN0_AFBCE_MMU_NUM);
		} else if (devp->dtdata->hw_ver == VDIN_HW_A9) {
			vfe->vf.afbce_num = rd(devp->addr_offset_afbc_a9, VDIN0_A9_AFBCE_MMU_NUM);
		} else {
			vfe->vf.afbce_num = rd(devp->addr_offset, AFBCE_MMU_NUM);
		}
		if (vfe->vf.afbce_num < VDIN_AFBCE_MIN_SIZE) {
			vfe->vf.afbce_num = devp->msct_top.mmu_4k_number * 7 / 10;
		}
		idx = devp->msct_top.cur_frame_idx;
		devp->msct_top.sct_stat[idx].compressed_page_cnt = vfe->vf.afbce_num;
		devp->msct_top.vfe = vfe;

		if (devp->debug.sct_print_ctl & SCT_PRINT_CTL_MMU_NUM)
			pr_info("vdin%d vf:%d afbce_num:%d;cur_idx:%d,cmp:%d\n",
				devp->index, vfe->vf.index, vfe->vf.afbce_num,
				devp->msct_top.cur_frame_idx,
				devp->msct_top.sct_stat[idx].compressed_page_cnt);
	}
}

int vdin_sct_start(struct vdin_dev_s *devp)
{
	int retry = 3, ret = 0;
	int err = 0;
	struct vf_entry *vfe = NULL;

	if (devp->mem_type != VDIN_MEM_TYPE_SCT)
		return 0;

	if (!devp->afbce_info) {
		err = 0;
		goto error;
	}

	devp->msct_top.mmu_4k_number = devp->afbce_info->frame_table_size / 4;

	//alloc one vf scatter memory with full size.
	vfe = provider_vf_peek(devp->vfp);
	if (!vfe) {
		err = -2;
		goto error;
	}
	while (retry--) {
		ret = vdin_sct_alloc(devp, vfe->vf.index);
		if (ret >= 0) {
			vfe->sct_stat = VFRAME_SCT_STATE_FULL;
			break;
		}
		msleep(50);
	}

	return 0;
error:
	pr_err("vdin sct start error:%d", err);
	return err;
}

int vdin_sct_alloc(struct vdin_dev_s *devp, int vf_idx)
{
	int ret;
	unsigned long page_idx;
	struct vdin_msct_top_s *psct;
	int cur_mmu_4k_number;
	u64 timer_st, timer_end, diff;

	psct = &devp->msct_top;
	timer_st = cur_to_usecs();

	cur_mmu_4k_number = devp->msct_top.mmu_4k_number;

	ret = vdin_mmu_box_alloc_idx(psct->box, vf_idx, cur_mmu_4k_number,
			 devp->afbce_info->fm_table_vir_paddr[vf_idx]);
	if (ret == 0) {
		page_idx = *(unsigned long *)devp->afbce_info->fm_table_vir_paddr[vf_idx];
		devp->afbce_info->fm_body_paddr[vf_idx] = (page_idx << PAGE_SHIFT);
		devp->msct_top.sct_stat[vf_idx].cur_page_cnt = cur_mmu_4k_number;
	} else {
		pr_err("vdin%d sct alloc error:%d\n", devp->index, ret);
	}

	codec_mm_dma_flush(devp->afbce_info->fm_table_vir_paddr[vf_idx],
			devp->afbce_info->frame_table_size, DMA_TO_DEVICE);

	timer_end = cur_to_usecs();
	diff = timer_end - timer_st;
	if (devp->debug.sct_print_ctl & SCT_PRINT_CTL_ALLOC_IDX)
		pr_info("vdin%d sct alloc size:%#x, num=%d, vf_id:%d(%p) use %uus (%s)\n",
			devp->index, devp->vf_mem_size, cur_mmu_4k_number,
			vf_idx, devp->afbce_info->fm_table_vir_paddr[vf_idx],
			(unsigned int)diff, (diff - 12000) ? "too long" : "");

	return ret;
}

void vdin_sct_free_tail(struct vdin_dev_s *devp, int vf_idx, int buffer_used)

{
	struct vdin_msct_top_s	*psct;
	int ret;
	u64 timer_st, timer_end, diff;
	int err = 0;
	timer_st = cur_to_usecs();
	psct = &devp->msct_top;

	if (!buffer_used) {
		err = -1;
		if (devp->debug.sct_print_ctl & SCT_PRINT_CTL_WARN)
			goto error;
		return;
	}

	if (buffer_used > devp->msct_top.mmu_4k_number) {
		err = -2;
		goto error;
	}
	ret = vdin_mmu_box_free_idx_tail(psct->box, vf_idx, buffer_used);
	if (ret == 0)
		psct->sct_stat[vf_idx].cur_page_cnt = buffer_used;
	else
		goto error;

	timer_end = cur_to_usecs();
	diff = timer_end - timer_st;
	if (devp->debug.sct_print_ctl & SCT_PRINT_CTL_FREE_TAIL)
		pr_info("vdin%d sct free vf_id:%x buf_used:%d use %uus 0x%x\n",
		devp->index, vf_idx, buffer_used, (unsigned int)diff, buffer_used);
	return;
error:
	pr_err("sct free tail error:%d vf_idx:%d buf_used:%d max_mmu:%u cur_idx:%u last_idx:%u\n",
		err, vf_idx, buffer_used, devp->msct_top.mmu_4k_number,
		devp->msct_top.cur_frame_idx, devp->msct_top.last_frame_idx);
	return;
}

//void vdin_sct_free(struct vdin_dev_s *devp, int index)
void vdin_sct_free(struct vf_pool *p, int index)
{
	int ret;
	u64 timer_st, timer_end, diff;
	struct vdin_msct_top_s *psct;
	struct vdin_dev_s *devp = NULL;
	int err  = 0;
	devp = (struct vdin_dev_s *)p->priv;

	if (devp->mem_type != VDIN_MEM_TYPE_SCT)
		return;

	psct = &devp->msct_top;

	timer_st = cur_to_usecs();
	if (index > psct->max_buf_num) {
		err = -1;
		goto error;
	}
	ret = vdin_mmu_box_free_idx(psct->box, index);
	if (ret == 0) {
		devp->afbce_info->fm_body_paddr[index] = 0;
		psct->sct_stat[index].cur_page_cnt = 0;
	} else {
		goto error;
	}

	timer_end = cur_to_usecs();
	diff = timer_end - timer_st;
	if (devp->debug.sct_print_ctl & SCT_PRINT_CTL_FREE_IDX)
		pr_info("vdin%d sct free id:%d use %uus (%s)\n",
			devp->index, index, (unsigned int)diff,
			((unsigned int)diff - 10000) ? "too long" : "");
	return;
error:
	pr_err("vdin sct free tail error:%d", err);
	return;
}

int vdin_sct_init(struct vdin_dev_s *devp)
{
	int tvp_flag = 0;
	int buf_size = devp->dts_config.sct_cache_size;
	unsigned int need_cache_size;
	struct vdin_msct_top_s *psct;
	int err = 0;

	vdin_mmu_box_init();

	psct = &devp->msct_top;
	if (psct->box) {
		err = -1;
		goto error;
	}
	need_cache_size = buf_size * SZ_1M;
	psct->sc_start_time = get_jiffies_64();
	psct->max_buf_num = VDIN_CANVAS_MAX_CNT;

	psct->box = vdin_mmu_box_alloc_box(DRIVER_NAME,
			devp->index, psct->max_buf_num,
			need_cache_size, tvp_flag);
	if (!psct->box) {
		err = -2;
		goto error;
	}

	if (devp->debug.sct_print_ctl & SCT_PRINT_CTL_INIT)
		pr_info("vdin%d sct int, tvp:0x%x, max_buf_num:%d, buf_size:%dMB\n",
			devp->index, tvp_flag, psct->max_buf_num, buf_size);

	return 0;
error:
	pr_err("vdin sct int error:%d", err);
	return err;
}

int vdin_sct_release(struct vdin_dev_s *devp)
{
	struct vdin_msct_top_s *psct;

	psct = &devp->msct_top;

	if (psct->box) {
		vdin_mmu_box_free(psct->box);
		psct->box = NULL;
	}

	return 0;
}

void vdin_sct_queue_work(struct vdin_dev_s *devp)
{
	if (devp->mem_type == VDIN_MEM_TYPE_SCT) {
		devp->msct_top.que_work_cnt++;
		if (devp->debug.sct_print_ctl & SCT_PRINT_CTL_QUE_WORK)
			pr_info("vdin%d sct queue irq:%d, frame:%d, q_work:%d, run:%d\n",
				devp->index, devp->irq_cnt, devp->frame_cnt,
				devp->msct_top.que_work_cnt, devp->msct_top.worker_run_cnt);
		queue_work(devp->wq, &devp->sct_work);
	}
}

/* when in game mode,try to free vf memory which in wr_list,except the first one */
void vdin_sct_free_idx_in_game(struct vdin_dev_s *devp)
{
	int i;
	ulong wr_list_flags = 0;
	struct vf_entry *vfe = NULL, *next_wr_vfe = NULL;

	for (i = 0; i < devp->vfp->size; i++) {
		vfe = vf_get_master(devp->vfp, i);
		if (!vfe) {
			pr_err("%s null vfe\n", __func__);
			break;
		}
		if (devp->debug.sct_print_ctl & SCT_PRINT_CTL_GAME)
			pr_info("[%d],vf_id:%d, sct_s:%d, vf_s:%d, cur_page_cnt:%d\n",
				i, vfe->vf.index, vfe->sct_stat, vfe->status,
				devp->msct_top.sct_stat[i].cur_page_cnt);
		//this vfe alloced memory for the next vsync,do not free it.
		next_wr_vfe = provider_vf_peek(devp->vfp);
		if (next_wr_vfe && vfe->vf.index == next_wr_vfe->vf.index)
			continue;
		/* donot free it, other vf still use it's memory in one buffer mode */
		if (vfe->vf.index == devp->af_num)
			continue;
		spin_lock_irqsave(&devp->vfp->wr_lock, wr_list_flags);
		if (vfe->status == VF_STATUS_WL &&
		    vfe->sct_stat == VFRAME_SCT_STATE_FULL) {
			vfe->sct_stat = VFRAME_SCT_STATE_INIT;
			spin_unlock_irqrestore(&devp->vfp->wr_lock, wr_list_flags);
			vdin_sct_free_tail(devp, vfe->vf.index, devp->dts_config.sct_remain_size);
		} else {
			spin_unlock_irqrestore(&devp->vfp->wr_lock, wr_list_flags);
		}
	}
}

void vdin_sct_worker(struct work_struct *work)
{
	int ret = 0;
	unsigned int idx;
	struct vf_entry *vfe = NULL, *next_wr_vfe = NULL;
	int err = 0;

	struct vdin_dev_s *devp =
		container_of(work, struct vdin_dev_s, sct_work);

	devp->msct_top.worker_run_cnt++;

	if (!devp->afbce_info) {
		err = -1;
		goto error;
	}
	if (devp->msct_top.sct_stop_flag) {
		err = -2;
		goto error;
	}
	/* alloc memory may take same time,save the captured vfe first */
	devp->msct_top.vfe = NULL;

	if (devp->debug.sct_print_ctl & SCT_PRINT_CTL_WORKER)
		pr_info("vdin sct worker: q_cnt:%d, run:%d\n",
			devp->msct_top.que_work_cnt, devp->msct_top.worker_run_cnt);

	//alloc memory for the next vfe.
	next_wr_vfe = provider_vf_peek(devp->vfp);
	if (next_wr_vfe) {
		if (devp->debug.sct_print_ctl & SCT_PRINT_CTL_WORKER)
			pr_info("vdin%d sct next vf_id:%d(%p), cur_page:%d, status:%d %d, af_num:%d\n",
				devp->index, next_wr_vfe->vf.index, next_wr_vfe->vf.mem_handle,
				devp->msct_top.sct_stat[next_wr_vfe->vf.index].cur_page_cnt,
				next_wr_vfe->sct_stat, next_wr_vfe->status, devp->af_num);
		if (next_wr_vfe->sct_stat != VFRAME_SCT_STATE_FULL) {
			ret = vdin_sct_alloc(devp, next_wr_vfe->vf.index);
			if (ret >= 0)
				next_wr_vfe->sct_stat = VFRAME_SCT_STATE_FULL;
		} else {
			if (devp->debug.sct_print_ctl & SCT_PRINT_CTL_WORKER)
				pr_info("vdin sct vf_id:%d, status:%d warning!!!\n",
					next_wr_vfe->vf.index, next_wr_vfe->sct_stat);
		}
		if (devp->af_num == VDIN_CANVAS_MAX_CNT)
			idx = next_wr_vfe->vf.index;
		else
			idx = devp->af_num;
		next_wr_vfe->vf.afbce_num = 0;
		next_wr_vfe->vf.mem_handle_1 =
			vdin_mmu_box_get_mem_handle(devp->msct_top.box, idx);
		if (devp->debug.sct_print_ctl & SCT_PRINT_CTL_MEM_HLD)
			pr_info("vdin%d,mem handle[%d]:%px %px\n",
			devp->index, next_wr_vfe->vf.index,
			next_wr_vfe->vf.mem_handle, next_wr_vfe->vf.mem_handle_1);
	} else {
		if (devp->debug.sct_print_ctl & SCT_PRINT_CTL_WARN)
			pr_info("vdin%d sct peek vframe failed,irq:%d,frame:%d;[%d %d %d %d]%d %d\n",
				devp->index, devp->irq_cnt, devp->frame_cnt,
				devp->vfp->wr_list_size, devp->vfp->wr_mode_size,
				devp->vfp->rd_list_size, devp->vfp->rd_mode_size,
				devp->msct_top.que_work_cnt, devp->msct_top.worker_run_cnt);
	}
	//alloc the next vfe memory end

	//free redundancy memory of captured vf.
	//do not free redundancy pages in game mode,free all pages of these in wr_list.
	vfe = vf_get_master(devp->vfp, devp->msct_top.last_frame_idx);
	if (vfe && next_wr_vfe && vfe->vf.index != next_wr_vfe->vf.index) {
		if (!(devp->debug.dbg_sct_ctl & DBG_SCT_CTL_NO_FREE_TAIL)) {
			idx = devp->msct_top.last_frame_idx;
			if (devp->debug.sct_print_ctl & SCT_PRINT_CTL_WORKER)
				pr_info("vdin%d sct vf:%d, afbc:%d, status:%d, id:%d %d, cmp:%d\n",
					devp->index, vfe->vf.index,
					vfe->vf.afbce_num, vfe->sct_stat,
					devp->msct_top.cur_frame_idx,
					devp->msct_top.last_frame_idx,
					devp->msct_top.sct_stat[idx].compressed_page_cnt);
			//free tail
			if (vfe->vf.index != devp->af_num) {
				vdin_sct_free_tail(devp, vfe->vf.index,
					devp->msct_top.sct_stat[idx].compressed_page_cnt);
				vfe->sct_stat = VFRAME_SCT_STATE_FREE_TAIL;
			}
		}
		if (devp->debug.sct_print_ctl & SCT_PRINT_CTL_WORKER)
			pr_info("vdin sct exit vf_id:%d, status:%d %d\n",
				vfe->vf.index, vfe->sct_stat, vfe->status);
	} else {
		if (devp->debug.sct_print_ctl & SCT_PRINT_CTL_WARN)
			pr_info("vdin sct no top vfe!!!\n");
	}
	//free redundancy memory of captured vf end
	return;
error:
	pr_err("vdin sct worker error:%d", err);
	return;
}

/* afbce use scatter or continuous memory */
int vdin_mem_init(struct vdin_dev_s *devp)
{
	int err = 0;

	if ((devp->debug.dbg_sct_ctl & DBG_SCT_CTL_DIS) ||
	    !vdin_is_afbce_enabled(devp) || (devp->index && devp->dtdata->hw_ver != VDIN_HW_A9) ||
	    !(devp->cma_config_flag & MEM_ALLOC_CODEC_SCT)) {
		devp->mem_type = VDIN_MEM_TYPE_CONTINUOUS;
		return 0;
	}

	/* scatter memory flag */
	devp->mem_type = VDIN_MEM_TYPE_SCT;

	if (!devp->get_vdin_mem_size_flag) {
		memset(&devp->msct_top, 0, sizeof(devp->msct_top));
		/* alloc box */
		vdin_sct_init(devp);

		devp->wq = alloc_ordered_workqueue("vdin0-worker",
			__WQ_LEGACY | WQ_MEM_RECLAIM | WQ_HIGHPRI);
		if (!devp->wq) {
			err = -EINVAL;
			goto error;
		}
		INIT_WORK(&devp->sct_work, vdin_sct_worker);
		//queue_work(devp->wq, &devp->sct_work);
		if (devp->debug.sct_print_ctl & SCT_PRINT_CTL_INIT)
			pr_info("%s vdin%d sct mem init done\n", __func__, devp->index);
	}

	return 0;
error:
	pr_err("vdin sct mem init error:%d", err);
	return err;
}

int vdin_mem_exit(struct vdin_dev_s *devp)
{
	int i;

	if (devp->mem_type != VDIN_MEM_TYPE_SCT)
		return 0;

	flush_workqueue(devp->wq);
	destroy_workqueue(devp->wq);

	for (i = 0; i < devp->vfp->size; i++)
		vdin_sct_free(devp->vfp, i);

	vdin_sct_release(devp);

	devp->frame_buff_num = devp->frame_buff_num_bak;

	if (devp->debug.sct_print_ctl & SCT_PRINT_CTL_EXIT)
		pr_info("vdin%d exit mem\n", devp->index);

	return 0;
}
