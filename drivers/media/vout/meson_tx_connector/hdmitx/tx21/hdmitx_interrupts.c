// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/rtc.h>
#include <linux/timekeeping.h>
#include <linux/gpio.h>
#include <linux/amlogic/media/vout/meson_tx_connector/hdmitx_common/hdmitx_common.h>
#include "hdmitx_hw_platform.h"
#include "hdmitx_hw_core.h"
#include "hdmitx_hdcp.h"
#include "meson_tx_hw_task.h"
#include "hdmitx_packet.h"
#include "meson_tx_event_mgr.h"

static void intr2_sw_handler(struct intr_t *, void *intr_para);
static void intr5_sw_handler(struct intr_t *, void *intr_para);
static void ddc_stall_req_handler(struct intr_t *intr, void *intr_para);
static void hdcp2x_intr_handler(struct intr_t *intr, void *intr_para);
static void hdcp1x_intr_handler(struct intr_t *intr, void *intr_para);

static struct hdmitx_intr hdmi_all_intr = {
	.top_intr = {
		.intr_mask_reg = HDMITX_TOP_INTR_MASK,
		.intr_st_reg = HDMITX_TOP_INTR_STAT,
		.intr_clr_reg = HDMITX_TOP_INTR_STAT_CLR,
		.intr_top_bit = BIT(1) | BIT(2),
		.mask_data = BIT(1) | BIT(2),
		/* handled in top half of irq */
		.callback = NULL,
	},
#ifdef CONFIG_AMLOGIC_HDCP_TX_MODERN
	.tpi_intr = { /* for hdcp1 */
		.intr_mask_reg = TPI_INTR_EN_IVCTX,
		.intr_st_reg = TPI_INTR_ST0_IVCTX,
		.intr_clr_reg = TPI_INTR_ST0_IVCTX,
		.intr_top_bit = BIT(0),
		.mask_data = 0xff,
		.callback = hdcp1x_intr_handler,
	},
	.cp2tx_intr0 = {
		.intr_mask_reg = CP2TX_INTR0_MASK_IVCTX,
		.intr_st_reg = CP2TX_INTR0_IVCTX,
		.intr_clr_reg = CP2TX_INTR0_IVCTX,
		.intr_top_bit = BIT(0),
		.mask_data = 0xFF,
		.callback = hdcp2x_intr_handler,
	},
	.cp2tx_intr1 = {
		.intr_mask_reg = CP2TX_INTR1_MASK_IVCTX,
		.intr_st_reg = CP2TX_INTR1_IVCTX,
		.intr_clr_reg = CP2TX_INTR1_IVCTX,
		.intr_top_bit = BIT(0),
		.mask_data = 0xFF,
		.callback = hdcp2x_intr_handler,
	},
	.cp2tx_intr2 = {
		.intr_mask_reg = CP2TX_INTR2_MASK_IVCTX,
		.intr_st_reg = CP2TX_INTR2_IVCTX,
		.intr_clr_reg = CP2TX_INTR2_IVCTX,
		.intr_top_bit = BIT(0),
		.mask_data = 0xFF,
		.callback = hdcp2x_intr_handler,
	},
	.cp2tx_intr3 = {
		.intr_mask_reg = CP2TX_INTR3_MASK_IVCTX,
		.intr_st_reg = CP2TX_INTR3_IVCTX,
		.intr_clr_reg = CP2TX_INTR3_IVCTX,
		.intr_top_bit = BIT(0),
		.mask_data = BIT(2) | BIT(3),
		.callback = hdcp2x_intr_handler,
	},
#endif
	.intr2 = {
		.intr_mask_reg = INTR2_MASK_SW_TPI_IVCTX,
		.intr_st_reg = INTR2_SW_TPI_IVCTX,
		.intr_clr_reg = INTR2_SW_TPI_IVCTX,
		.intr_top_bit = BIT(0),
		.mask_data = BIT(0),
		.callback = intr2_sw_handler,
	},
	.intr5 = {
		.intr_mask_reg = INTR5_MASK_SW_TPI_IVCTX,
		.intr_st_reg = INTR5_SW_TPI_IVCTX,
		.intr_clr_reg = INTR5_SW_TPI_IVCTX,
		.intr_top_bit = BIT(0),
		.mask_data = BIT(3),
		.callback = intr5_sw_handler,
	},
	/* not enable this interrupt, there are frequent poll_update_flags()
	 * under FRL mode which will stall request SCDC DDC, there will be
	 * lots of interrupts
	 */
	.scdc_intr = {
		.intr_mask_reg = SCDC_INTR0_MASK_IVCTX,
		.intr_st_reg = SCDC_INTR0_IVCTX,
		.intr_clr_reg = SCDC_INTR0_IVCTX,
		.intr_top_bit = BIT(0),
		.mask_data = BIT(5),
		.callback = ddc_stall_req_handler,
	},
};

#ifdef CONFIG_AMLOGIC_HDCP_TX_MODERN
void intr_status_save_clr_cp2txs(u8 regs[])
{
	regs[0] = hdmi_all_intr.cp2tx_intr0.st_data;
	regs[1] = hdmi_all_intr.cp2tx_intr1.st_data;
	regs[2] = hdmi_all_intr.cp2tx_intr2.st_data;
	regs[3] = hdmi_all_intr.cp2tx_intr3.st_data;
	hdmi_all_intr.cp2tx_intr0.st_data = 0;
	hdmi_all_intr.cp2tx_intr1.st_data = 0;
	hdmi_all_intr.cp2tx_intr2.st_data = 0;
	hdmi_all_intr.cp2tx_intr3.st_data = 0;
}

static void hdcp2x_intr_handler(struct intr_t *intr, void *intr_para)
{
	u8 hdcp2_intr_reg[4];
	struct hdmitx_common *tx_comm = (struct hdmitx_common *)intr_para;
	struct hdcptx21_core *p_hdcp;

	if (!tx_comm || !tx_comm->hdcptx_priv) {
		HDMITX_ERROR("%s NULL tx_comm or NULL hdcp_private instance\n", __func__);
		return;
	}
	p_hdcp = tx_comm->hdcptx_priv;

	intr_status_save_clr_cp2txs(hdcp2_intr_reg);
	hdcp2x_process_intr(p_hdcp, hdcp2_intr_reg);
}

static void hdcp1x_intr_handler(struct intr_t *intr, void *intr_para)
{
	u8 hdcp14_intr_reg;
	struct hdmitx_common *tx_comm = (struct hdmitx_common *)intr_para;
	struct hdcptx21_core *p_hdcp;

	if (!tx_comm || !tx_comm->hdcptx_priv) {
		HDMITX_ERROR("%s NULL tx_comm or NULL hdcp_private instance\n", __func__);
		return;
	}
	p_hdcp = tx_comm->hdcptx_priv;

	HDMITX_DEBUG_HDCP("%s[%d]\n", __func__, __LINE__);
	hdcp14_intr_reg = intr->st_data;
	/* clear intr state asap instead of clear after process intr */
	intr->st_data = 0;
	hdcp1x_process_intr(p_hdcp, hdcp14_intr_reg);
}
#endif

static void intr2_sw_handler(struct intr_t *intr, void *intr_para)
{
	static u32 vsync_cnt;

	/* clear intr state asap */
	intr->st_data = 0;
	vsync_cnt++;
	if (vsync_cnt % 64 == 0) {
		/* blank here */
		/* HDMITX_INFO("%s[%d] vsync_cnt %d\n", __func__, __LINE__, vsync_cnt); */
	}
}

/* for fifo intr which come after mode setting done, in order to
 * prevent fifo reset operation itself trigger new fifo over/under
 * flow, the interrupt process flow:
 * disable fifo intr in interrupt top handler-->do
 * fifo reset-->clear intr state twice-->
 * after enter interrupt bottom handler, do clear intr5 state
 * twice-->enable fifo intr.
 * there's delay between intr top and bottom half
 */
static void intr5_sw_handler(struct intr_t *intr, void *intr_para)
{
	/* clear intr state asap */
	intr->st_data = 0;
	/* clear pfifo intr twice */
	hdmitx21_set_reg_bits(INTR5_SW_TPI_IVCTX, 1, 3, 1);
	hdmitx21_set_reg_bits(INTR5_SW_TPI_IVCTX, 1, 3, 1);
	/* enable fifo intr */
	hdmitx_fifo_flow_enable_intr(true);
	HDMITX_INFO("%s INTR5_SW_TPI_IVCTX pfifo rst\n", __func__);
}

static void ddc_stall_req_handler(struct intr_t *intr, void *intr_para)
{
	/* clear intr state asap */
	intr->st_data = 0;
}

static void _intr_enable(struct intr_t *pint, bool en)
{
	hdmitx21_wr_reg(pint->intr_mask_reg, en ? pint->mask_data : 0);
	if (en)
		hdmitx21_set_bit(HDMITX_TOP_INTR_MASK, pint->intr_top_bit, en);
}

#ifdef CONFIG_AMLOGIC_HDCP_TX_MODERN
void hdcp_enable_intr(bool en)
{
	struct hdmitx21_dev *hdev = get_hdmitx21_device();

	if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S7 ||
		hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S7D ||
		hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S6) {
		if (hdev->tx_comm.hdcptx_comm.hdcp_mode == 1) {
			_intr_enable((struct intr_t *)&hdmi_all_intr.tpi_intr, en);
		} else if (hdev->tx_comm.hdcptx_comm.hdcp_mode == 2) {
			_intr_enable((struct intr_t *)&hdmi_all_intr.cp2tx_intr0, en);
			_intr_enable((struct intr_t *)&hdmi_all_intr.cp2tx_intr1, en);
			_intr_enable((struct intr_t *)&hdmi_all_intr.cp2tx_intr2, en);
			/* hpd rising/falling intr have no actual handle, no need to enable */
			/* _intr_enable((struct intr_t *)&hdmi_all_intr.cp2tx_intr3, en); */
		}
	} else {
		_intr_enable((struct intr_t *)&hdmi_all_intr.tpi_intr, en);
		_intr_enable((struct intr_t *)&hdmi_all_intr.cp2tx_intr0, en);
		_intr_enable((struct intr_t *)&hdmi_all_intr.cp2tx_intr1, en);
		_intr_enable((struct intr_t *)&hdmi_all_intr.cp2tx_intr2, en);
		/* hpd rising/falling intr have no actual handle, no need to enable */
		/* _intr_enable((struct intr_t *)&hdmi_all_intr.cp2tx_intr3, en); */
	}
}
#endif

void hdmitx_fifo_flow_enable_intr(bool en)
{
	if (en)
		hdmitx21_set_reg_bits(INTR5_SW_TPI_IVCTX, 1, 3, 1);
	_intr_enable((struct intr_t *)&hdmi_all_intr.intr5, en);
}

static void hdmitx_phy_band_gap_en(struct hdmitx21_dev *hdev)
{
	switch (hdev->tx_comm.tx_hw->chip_data->chip_type) {
	case MESON_CPU_ID_T7:
	case MESON_CPU_ID_S1A:
		hdmitx21_phy_band_gap_en_t7();
		break;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	case MESON_CPU_ID_S7:
		hdmitx21_phy_band_gap_en_s7();
		break;
	case MESON_CPU_ID_S7D:
		hdmitx21_phy_band_gap_en_s7d();
		break;
#endif
	default:
		break;
	}
}

void hdmitx_hpd_irq_top_half_process(struct hdmitx21_dev *hdev, bool hpd)
{
	if (hpd) {
		hdmitx_phy_band_gap_en(hdev);
		if (hdev->tx_comm.earc_hdmitx_hpdst)
			hdev->tx_comm.earc_hdmitx_hpdst(true);
	} else {
		if (hdev->tx_comm.earc_hdmitx_hpdst)
			hdev->tx_comm.earc_hdmitx_hpdst(false);
	}
}

void hdmitx_core_intr_process(struct hdmitx_common *tx_comm)
{
	int i;
	struct intr_t *pint = (struct intr_t *)&hdmi_all_intr;
	u32 val;
	struct hdmitx21_dev *hdev;

	if (!tx_comm)
		return;

	hdev = container_of(tx_comm, struct hdmitx21_dev, tx_comm);
	/* note: the callback sequence is as the member sequence of
	 * struct hdmitx_intr, instead of the sequence of hdmi_all_intr
	 */
	for (i = 1; i < sizeof(struct hdmitx_intr) / sizeof(struct intr_t); i++) {
		pint++;
		/* HDMITX_INFO("-----i = %d, pint->st_data = 0x%x\n", i, pint->st_data); */
		/* only process the enabled interrupt */
		if (((hdmitx21_rd_reg(pint->intr_mask_reg) & pint->mask_data) ||
			(pint->intr_mask_reg == INTR5_MASK_SW_TPI_IVCTX &&
			!hdev->ignore_fifo_intr5)) &&
			(pint->st_data & pint->mask_data)) {
			val = pint->st_data;
			/* clear st_data asap in callback function */
			if (pint->callback)
				pint->callback(pint, (void *)tx_comm);
		} else {
			pint->st_data = 0;
		}
	}
}

/* there may be such case: after plugout interrupt come and save its
 * interrupt state in st_data, before execute its bottom handle, there's
 * another INTR5 come, then will do RE_ISR in intr_handler(), it will read
 * plugin/out interrupt state and over-write the previous saved plug
 * interrupt state in st_data. when execute the bottom handle of interrupt,
 * and find that the plug interrupt is cleared, so won't do plug process
 */
static void intr_status_save_and_clear(void)
{
	struct intr_t *pint = (struct intr_t *)&hdmi_all_intr;
#ifdef CONFIG_AMLOGIC_HDCP_TX_MODERN
	struct hdmitx21_dev *hdev = get_hdmitx21_device();
	u32 gate_status = hdmitx21_rd_reg(HDMITX_TOP_CLK_GATE);
#endif
	int i;
	u32 tmp;

	for (i = 0; i < sizeof(struct hdmitx_intr) / sizeof(struct intr_t); i++, pint++) {
#ifdef CONFIG_AMLOGIC_HDCP_TX_MODERN
		if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S7 ||
			hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S7D ||
			hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S6) {
			if (!(gate_status & BIT_HDMITX_TOP_CLK_GATE_HDCP1X)) {
				if (pint->intr_st_reg == TPI_INTR_ST0_IVCTX)
					continue;
			}
			if (!(gate_status & BIT_HDMITX_TOP_CLK_GATE_HDCP2X)) {
				if (pint->intr_st_reg == CP2TX_INTR0_IVCTX ||
					pint->intr_st_reg == CP2TX_INTR1_IVCTX ||
					pint->intr_st_reg == CP2TX_INTR2_IVCTX ||
					pint->intr_st_reg == CP2TX_INTR3_IVCTX) {
					continue;
				}
			}
		}
#endif
		/* if last bottom half of interrupt is not finished, should keep
		 * the interrupt state, only clear it after it is already
		 * used in bottom half of interrupt handle
		 */
		tmp = hdmitx21_rd_reg(pint->intr_st_reg);
		pint->st_data |= tmp;

		if (pint->intr_st_reg == INTR5_SW_TPI_IVCTX &&
			(hdmitx21_rd_reg(pint->intr_mask_reg) & pint->mask_data) &&
			(pint->st_data & pint->mask_data)) {
			/* disable intr5 */
			hdmitx_fifo_flow_enable_intr(false);
			/* reset fifo */
			hdmitx21_set_reg_bits(PWD_SRST_IVCTX, 1, 2, 1);
			hdmitx21_set_reg_bits(PWD_SRST_IVCTX, 0, 2, 1);
			/* clear pfifo intr twice */
			hdmitx21_set_reg_bits(INTR5_SW_TPI_IVCTX, 1, 3, 1);
			hdmitx21_set_reg_bits(INTR5_SW_TPI_IVCTX, 1, 3, 1);
		} else {
			hdmitx21_wr_reg(pint->intr_clr_reg, pint->st_data);
		}
	}
}

void intr_status_init_clear(void)
{
	int i;
	u32 st_data;
	struct intr_t *pint = (struct intr_t *)&hdmi_all_intr;
#ifdef CONFIG_AMLOGIC_HDCP_TX_MODERN
	struct hdmitx21_dev *hdev = get_hdmitx21_device();
	u32 gate_status = hdmitx21_rd_reg(HDMITX_TOP_CLK_GATE);
#endif

	for (i = 0; i < sizeof(struct hdmitx_intr) / sizeof(struct intr_t); i++, pint++) {
#ifdef CONFIG_AMLOGIC_HDCP_TX_MODERN
		if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S7 ||
			hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S7D ||
			hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S6) {
			if (!(gate_status & BIT_HDMITX_TOP_CLK_GATE_HDCP1X)) {
				if (pint->intr_st_reg == TPI_INTR_ST0_IVCTX)
					continue;
			}
			if (!(gate_status & BIT_HDMITX_TOP_CLK_GATE_HDCP2X)) {
				if (pint->intr_st_reg == CP2TX_INTR0_IVCTX ||
					pint->intr_st_reg == CP2TX_INTR1_IVCTX ||
					pint->intr_st_reg == CP2TX_INTR2_IVCTX ||
					pint->intr_st_reg == CP2TX_INTR3_IVCTX) {
					continue;
				}
			}
		}
#endif
		st_data = hdmitx21_rd_reg(pint->intr_st_reg);
		hdmitx21_wr_reg(pint->intr_clr_reg, st_data);
		pint->st_data = 0;
	}
}

static void hdmitx_top_intr_handler(struct hdmitx21_dev *hdev, struct intr_t *top_intr)
{
	struct hdmitx_hw_common *hw_comm = NULL;
	struct meson_tx_hw *tx_hw_base = NULL;
	bool ret = false;

	if (!hdev || !top_intr) {
		HDMITX_ERROR("%s NULL instance or intr\n", __func__);
		return;
	}
	hw_comm = &hdev->hw_comm;
	tx_hw_base = &hw_comm->tx_hw_base;

	if (top_intr->st_data) {
		u32 dat_top;

		dat_top = top_intr->st_data;
		/* clear intr state asap */
		top_intr->st_data = 0;
		/* check HPD status */
		if (!hdev->tx_comm.pxp_mode && ((dat_top & (1 << 1)) && (dat_top & (1 << 2)))) {
			if (hdmitx21_hpd_hw_op(&hdev->tx21_hw, HPD_READ_HPD_GPIO))
				dat_top &= ~(1 << 2);
			else
				dat_top &= ~(1 << 1);
		}
		/* bit[2:1] of dat_top means HPD falling and rising */
		if ((dat_top & 0x6) && hdev->hw_comm.hdmitx_gpios_hpd >= 0) {
			struct timespec64 kts;
			struct rtc_time tm;

			ktime_get_real_ts64(&kts);
			rtc_time64_to_tm(kts.tv_sec, &tm);
			HDMITX_INFO("UTC+0 %ptRd %ptRt HPD %s\n", &tm, &tm,
				gpio_get_value(hdev->hw_comm.hdmitx_gpios_hpd) ? "HIGH" : "LOW");
		}
		if ((dat_top & 0x6) && hdev->hw_comm.debug_hpd_lock) {
			HDMITX_INFO("HDMI hpd locked\n");
			return;
		}
		/* HPD rising */
		if (dat_top & (1 << 1)) {
			hdmitx_hpd_irq_top_half_process(hdev, true);
			ret = tx_hw_base->event_ops->queue_event(tx_hw_base->event_ops->data,
				HPD_PLUGIN, msecs_to_jiffies(500));
			if (!ret) {
				HDMITX_DEBUG_HPD("too much plugin, send TX_LINK_UNSTABLE uevent\n");
				hdmitx_set_uevent(&hdev->tx_comm, TX_LINK_UNSTABLE, 1);
			}
		}
		/* HPD falling */
		if (dat_top & (1 << 2)) {
			hdmitx_hpd_irq_top_half_process(hdev, false);
			/* Cancel previous hpd work.
			 * Note that plugout work is not canceled so as to
			 * prevent plugout work is not scheduled asap in
			 * critical high cpu loading case. always do
			 * plugout work to disable output asap.
			 */
			ret = tx_hw_base->event_ops->cancel_event(tx_hw_base->event_ops->data,
				HPD_PLUGIN, false);
			if (ret)
				HDMITX_DEBUG_HPD("plugin work is pending and canceled\n");
			else
				HDMITX_DEBUG_HPD("plugin work is not pending\n");
			ret = tx_hw_base->event_ops->queue_event(tx_hw_base->event_ops->data,
				HPD_PLUGOUT, 0);
			if (!ret) {
				HDMITX_INFO("too much plugout, send TX_LINK_UNSTABLE uevent\n");
				hdmitx_set_uevent(&hdev->tx_comm, TX_LINK_UNSTABLE, 1);
			}
		}
	}
}

static irqreturn_t intr_handler(int irq, void *dev)
{
	unsigned int top_intr_state;
	struct hdmitx21_dev *hdev = (struct hdmitx21_dev *)dev;
	struct hdmitx_hw_common *hw_comm = &hdev->hw_comm;
	struct intr_t *pint = (struct intr_t *)&hdmi_all_intr;

RE_ISR:
	intr_status_save_and_clear();
	top_intr_state = hdmitx21_rd_reg(HDMITX_TOP_INTR_STAT);

	/* irq handler for hdmi top(HPD out/in) */
	hdmitx_top_intr_handler(hdev, pint);
	/*
	 * irq handler for hdmi core,
	 * for hdcp cts test, need handle ASAP w/o any delay
	 */
	hw_comm->tx_hw_base.event_ops->queue_event(hw_comm->tx_hw_base.event_ops->data,
		CORE_TASK, 0);

	/* if TX Controller interrupt shadowing is true,
	 * it means there's interrupt not cleared/handled
	 * so need to handle it before exit interrupt handler
	 */
	if (top_intr_state & BIT(2) ||
		top_intr_state & BIT(1) ||
		top_intr_state & BIT(0)) {
		HDMITX_INFO("interrupt not cleared, re-handle intr 0x%x\n", top_intr_state);
		goto RE_ISR;
	}
	return IRQ_HANDLED;
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static irqreturn_t vrr_vsync_intr_handler(int irq, void *dev)
{
	return hdmitx_vrr_vsync_handler((struct hdmitx21_dev *)dev);
}
#endif

static irqreturn_t vsync_intr_handler(int irq, void *dev)
{
	struct hdmitx21_dev *hdev = (struct hdmitx21_dev *)dev;
	u32 arg;

	/*
	 * Only in cuva receiver mode, when you need to output cuva signals, you
	 * need to reset cuva d_hdr every frame
	 */
	hdmitx_cuva_dhdr_reset(&hdev->tx_comm);

	if (hdev->tx_comm.hdr_state->vid_mute_op != VIDEO_NONE_OP) {
		arg = hdev->tx_comm.hdr_state->vid_mute_op;
		hdmitx_hw_cntl(&hdev->hw_comm,
			VPU_VIDEO_MUTE_OP, (void *)&arg, NULL);
		hdev->tx_comm.hdr_state->vid_mute_op = VIDEO_NONE_OP;
	}

	if (hdev->tx_comm.tx_hw->tmds_phy_op == TMDS_PHY_DISABLE) {
		arg = hdev->tx_comm.tx_hw->tmds_phy_op;
		hdmitx_hw_cntl(&hdev->hw_comm,
			PLATFORM_PHY_OP, (void *)&arg, NULL);
		hdev->tx_comm.tx_hw->tmds_phy_op = TMDS_PHY_NONE;
	}

	if (hdev->tx_comm.hdr_state->csc_config_in_next_frame) {
		/*
		 * avi infoframe takes effect on the next frame, so it needs
		 * to be updated one frame in advance
		 */
		if (hdev->tx_comm.hdr_state->csc_delay_frame == CSC_DELAY_FRAME - 1)
			hdmitx21_csc_update_avi_infoframe(hdev,
				hdev->tx_comm.hdr_state->output_color_format);
		/* after csc is configured, the frame takes effect */
		if (hdev->tx_comm.hdr_state->csc_delay_frame == CSC_DELAY_FRAME) {
			hdev->tx_comm.hdr_state->csc_config_in_next_frame = false;
			hdmitx21_color_convert(hdev, hdev->tx_comm.hdr_state->output_color_format);
		}
		hdev->tx_comm.hdr_state->csc_delay_frame += 1;
	}

	return IRQ_HANDLED;
}

#ifdef CONFIG_AMLOGIC_DSC
static irqreturn_t emp_vsync_intr_handler(int irq, void *dev)
{
	return hdmitx_emp_vsync_handler((struct hdmitx21_dev *)dev);
}
#endif

/* hdmitx shutdown and disable all irq */
void hdmitx21_disable_all_irq(void)
{
	_intr_enable((struct intr_t *)&hdmi_all_intr.top_intr, false);
	_intr_enable((struct intr_t *)&hdmi_all_intr.intr2, false);
	_intr_enable((struct intr_t *)&hdmi_all_intr.intr5, false);
#ifdef CONFIG_AMLOGIC_HDCP_TX
	_intr_enable((struct intr_t *)&hdmi_all_intr.tpi_intr, false);
	_intr_enable((struct intr_t *)&hdmi_all_intr.cp2tx_intr0, false);
	_intr_enable((struct intr_t *)&hdmi_all_intr.cp2tx_intr1, false);
	_intr_enable((struct intr_t *)&hdmi_all_intr.cp2tx_intr2, false);
	_intr_enable((struct intr_t *)&hdmi_all_intr.cp2tx_intr3, false);
#endif
	_intr_enable((struct intr_t *)&hdmi_all_intr.scdc_intr, false);

	HDMITX_INFO("disable hdmitx all irq");
}

int hdmitx_setup_irqs(struct hdmitx_hw_common *tx_hw)
{
	int r;
	struct hdmitx21_dev *phdev = get_hdmitx21_device();

	if (phdev->tx_comm.pxp_mode)
		return -EINVAL;

	hdmitx21_wr_reg(HDMITX_TOP_INTR_STAT_CLR, 0x7);
	r = request_irq(phdev->tx_comm.irq_hpd, &intr_handler,
			IRQF_SHARED, "hdmitx",
			(void *)phdev);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	r = request_irq(phdev->tx_comm.irq_hdmitx_vsync, &vrr_vsync_intr_handler,
			IRQF_SHARED, "hdmitx_vrr_vsync",
			(void *)phdev);
	if (r != 0)
		HDMITX_INFO(SYS "can't request vrr_vsync irq\n");
#endif
	r = request_irq(phdev->tx_comm.irq_hdmitx_vsync, &vsync_intr_handler,
			IRQF_SHARED, "hdmi_vsync",
			(void *)phdev);
	if (r != 0)
		HDMITX_INFO(SYS "can't request hdmi_vsync irq\n");
#ifdef CONFIG_AMLOGIC_DSC
	r = request_irq(phdev->tx_comm.irq_hdmitx_vsync, &emp_vsync_intr_handler,
			IRQF_SHARED, "hdmitx_emp_vsync",
			(void *)phdev);
	if (r != 0)
		HDMITX_INFO(SYS "can't request emp_vsync irq\n");
#endif
	return r;
}
