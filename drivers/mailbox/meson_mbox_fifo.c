// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/mailbox_client.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/of_device.h>
#include <linux/kthread.h>
#include <linux/amlogic/aml_mbox.h>
#include <dt-bindings/mailbox/t3-mbox.h>
#include <dt-bindings/mailbox/t7-mbox.h>
#include <dt-bindings/mailbox/sc2-mbox.h>
#include <dt-bindings/mailbox/s4-mbox.h>
#include <dt-bindings/mailbox/t5m-mbox.h>
#include <dt-bindings/mailbox/t3x-mbox.h>
#include <dt-bindings/mailbox/s5-mbox.h>
#include <dt-bindings/mailbox/s6-mbox.h>
#include <dt-bindings/mailbox/s1a-mbox.h>
#include <dt-bindings/mailbox/a4-mbox.h>
#include <dt-bindings/mailbox/a5-mbox.h>
#include <dt-bindings/mailbox/s7-mbox.h>
#include <dt-bindings/mailbox/s7d-mbox.h>
#include <dt-bindings/mailbox/t6d-mbox.h>
#include <dt-bindings/mailbox/t6w-mbox.h>
#include <dt-bindings/mailbox/t6x-mbox.h>
#include "meson_mbox_fifo.h"
#include "meson_mbox_comm.h"

#define ASYNC	1
#define SYNC	2
#define MBOX_HEAD_SIZE	0x1C
#define DRIVER_NAME	"meson_mbox_fifo"

struct mbox_header {
	u32 status;
	u64 task;
	u64 complete;
	u64 ullclt;
} __packed;

struct mbox_data {
	struct mbox_header mbox_header;
	char data[MBOX_DATA_SIZE];
} __packed;

static void mbox_fifo_write(void __iomem *to, void *from, long count)
{
	int i = 0;
	long len = count / 4;
	long rlen = count % 4;
	u32 rdata = 0;
	u32 *p = (u32 *)from;

	while (len > 0) {
		writel(*(const u32 *)p, (to + 4 * i));
		len--;
		i++;
		p++;
	}

	if (rlen != 0) {
		memcpy(&rdata, p, rlen);
		writel(rdata, (to + 4 * i));
	}
}

static void mbox_fifo_read(void *to, void __iomem *from, long count)
{
	int i = 0;
	long len = count / 4;
	long rlen = count % 4;
	u32 rdata = 0;
	u32 *p = to;

	while (len > 0) {
		*p = readl(from + (4 * i));
		len--;
		i++;
		p++;
	}

	if (rlen != 0) {
		rdata = readl(from + (4 * i));
		memcpy(p, &rdata, rlen);
	}
}

static void mbox_fifo_clr(void __iomem *to, long count)
{
	int i = 0;
	int len = (count + 3) / 4;

	while (len > 0) {
		writel(0, (to + 4 * i));
		len--;
		i++;
	}
}

static void mbox_irq_clean(struct aml_mbox_chan *aml_chan, bool ack)
{
	u64 h_sts, l_sts;
	u64 mask;

	if (ack)
		mask = IRQ_SENDACK_BIT(aml_chan->mboxid);
	else
		mask = IRQ_REV_BIT(aml_chan->mboxid);
	if (aml_chan->irq_nums / MHUIRQ_MAXNUM_DEF == 2) {
		h_sts = (mask >> MBOX_IRQSHIFT) & MBOX_IRQMASK;
		l_sts = mask & MBOX_IRQMASK;
		writel(l_sts, aml_chan->mbox_irqclr_addr);
		writel(h_sts, aml_chan->mbox_irqclr_addr + 4);
	} else {
		writel((mask & MBOX_IRQMASK), aml_chan->mbox_irqclr_addr);
	}
}

static void mbox_wakeup_wait_task(void *mssg)
{
	struct aml_mbox_data *aml_rdata = mssg;
	struct completion *p_comp = (struct completion *)(unsigned long)aml_rdata->rev_complete;
	struct aml_mbox_data *aml_sdata = container_of(p_comp, struct aml_mbox_data, complete);
	int rxsize;

	if (IS_ERR_OR_NULL(p_comp))
		return;

	if (aml_rdata->rxsize && aml_rdata->rxbuf) {
		rxsize = min(aml_sdata->rxsize, aml_rdata->rxsize);
		aml_sdata->rxsize = aml_rdata->rxsize;
		memcpy(aml_sdata->rxbuf, aml_rdata->rxbuf, rxsize);
	}
	complete(p_comp);
}

static int mbox_chan_id_map(struct device *dev, int drvid)
{
	struct aml_priv_data *mbox_priv_data;
	struct mbox_domain_data *domain_data;
	const struct mbox_domain *mbox_domains;
	int idx;
	int chan_nums;

	mbox_priv_data = dev_get_drvdata(dev);
	domain_data = &mbox_priv_data->domain_data;
	mbox_domains = domain_data->mbox_domains;
	chan_nums = domain_data->domain_counts;
	for (idx = 0; idx < chan_nums; idx++) {
		if (drvid == mbox_domains[idx].drvid)
			break;
	}

	if (idx == chan_nums)
		return -EINVAL;

	return idx;
}

static struct mbox_chan *mbox_chan_xlate(struct mbox_controller *mbox,
		    const struct of_phandle_args *sp)
{
	struct device *dev = mbox->dev;
	int drv_id = sp->args[0];
	int chan_id;

	if (drv_id > MBOX_ID_MAX) {
		dev_err(dev, "mbox driver id %d exceed maximum id\n", drv_id);
		return ERR_PTR(-EINVAL);
	}

	chan_id = mbox_chan_id_map(dev, drv_id);
	if (chan_id < 0) {
		dev_err(dev, "failed to get mbox chan id\n");
		return ERR_PTR(-EINVAL);
	}

	dev_dbg(dev, "chan xlate, drv_id = %d, chan_id = %d\n",
			drv_id, chan_id);
	return &mbox->chans[chan_id];
}

static int mbox_chk_valid_drv_id(int drvid)
{
	int ret;

	if (drvid >= MBOX_DRVID_AO2ARMREE_BEGIN &&
			drvid <= MBOX_DRVID_AO2ARMREE_END)
		ret = 0;
	else if (drvid >= MBOX_DRVID_DSP2ARMREE_BEGIN &&
			drvid <= MBOX_DRVID_DSP2ARMREE_END)
		ret = 0;
	else if (drvid >= MBOX_DRVID_M42ARMREE_BEGIN &&
			drvid <= MBOX_DRVID_M42ARMREE_END)
		ret = 0;
	else
		ret = 1;

	return ret;
}

static int mbox_rx_msg_submit(struct device *dev)
{
	struct aml_mbox_rx_packet *rx_packet;
	struct aml_mbox_rx_data mbox_rx_data;
	struct mbox_chan *mbox_chan;
	struct mbox_controller *mbox_ctrl;
	struct aml_priv_data *mbox_priv_data;
	struct aml_mbox_rx_msg *mbox_rx_msg;
	int chan_id;
	unsigned long flags;
	unsigned int idx;
	unsigned int count;
	u32 rx_queue_len;

	mbox_priv_data = dev_get_drvdata(dev);
	mbox_ctrl = mbox_priv_data->mbox_ctrl;
	mbox_rx_msg = mbox_priv_data->rx_msg;
	rx_queue_len = mbox_rx_msg->rx_queue_len;

	spin_lock_irqsave(&mbox_rx_msg->lock, flags);
	if (!mbox_rx_msg->msg_count) {
		spin_unlock_irqrestore(&mbox_rx_msg->lock, flags);
		return -ENODATA;
	}

	count = mbox_rx_msg->msg_count;
	idx = mbox_rx_msg->msg_free;
	if (idx >= count)
		idx -= count;
	else
		idx += rx_queue_len - count;

	rx_packet = &mbox_rx_msg->msg_data[idx];
	chan_id = rx_packet->rx_header.chan_id;
	mbox_rx_data.cmd = rx_packet->rx_header.cmd;
	mbox_rx_data.size = rx_packet->rx_header.size;
	memset(mbox_rx_data.buf, 0, MBOX_DATA_SIZE);
	memcpy(mbox_rx_data.buf, rx_packet->buf, rx_packet->rx_header.size);
	mbox_rx_msg->msg_count--;
	spin_unlock_irqrestore(&mbox_rx_msg->lock, flags);

	if (chan_id < 0) {
		dev_err(dev, "Error: get wrong channel ID\n");
		return -EINVAL;
	}

	dev_dbg(dev, "%s: send chan_id %d, cmd 0x%x to client driver\n",
			__func__, chan_id, mbox_rx_data.cmd);
	mbox_chan = &mbox_ctrl->chans[chan_id];
	/* transfer data to client driver */
	mbox_chan_received_data(mbox_chan, &mbox_rx_data);
	dev_dbg(dev, "%s: chan_id %d, cmd 0x%x callback done\n",
			__func__, chan_id, mbox_rx_data.cmd);

	return 0;
}

static int mbox_rx_process(void *data)
{
	struct device *dev = (struct device *)data;
	struct aml_priv_data *mbox_priv_data = dev_get_drvdata(dev);
	struct aml_mbox_rx_msg *mbox_rx_msg = mbox_priv_data->rx_msg;

	while (!kthread_should_stop()) {
		if (down_interruptible(&mbox_rx_msg->sem))
			continue;

		while (mbox_rx_msg_submit(dev) != -ENODATA)
			;
	}

	return 0;
}

static int mbox_add_rx_buf(struct device *dev, int chan_id,
		struct aml_mbox_data *aml_data)
{
	struct aml_mbox_rx_packet *rx_packet;
	struct aml_priv_data *mbox_priv_data;
	struct aml_mbox_rx_msg *mbox_rx_msg;
	unsigned int idx;
	unsigned long flags;
	u32 rx_queue_len;

	mbox_priv_data = dev_get_drvdata(dev);
	mbox_rx_msg = mbox_priv_data->rx_msg;

	spin_lock_irqsave(&mbox_rx_msg->lock, flags);
	/* See if there is any space left in rx queue */
	rx_queue_len = mbox_rx_msg->rx_queue_len;
	if (mbox_rx_msg->msg_count == rx_queue_len) {
		spin_unlock_irqrestore(&mbox_rx_msg->lock, flags);
		dev_err(dev, "Error: RX buffer full, chan_id %d msg dropped\n",
				chan_id);
		return -ENOBUFS;
	}

	idx = mbox_rx_msg->msg_free;
	rx_packet = &mbox_rx_msg->msg_data[idx];
	rx_packet->rx_header.chan_id = chan_id;
	rx_packet->rx_header.cmd = aml_data->cmd;
	rx_packet->rx_header.size = aml_data->rxsize;
	memset(rx_packet->buf, 0, MBOX_DATA_SIZE);
	memcpy(rx_packet->buf, aml_data->rxbuf, aml_data->rxsize);
	mbox_rx_msg->msg_count++;
	if (idx == rx_queue_len - 1)
		mbox_rx_msg->msg_free = 0;
	else
		mbox_rx_msg->msg_free++;
	spin_unlock_irqrestore(&mbox_rx_msg->lock, flags);

	return idx;
}

static void mbox_add_rx_queue(struct device *dev, int drv_id,
		struct aml_mbox_data *aml_data)
{
	struct aml_priv_data *mbox_priv_data;
	struct aml_mbox_rx_msg *mbox_rx_msg;
	struct mbox_controller *mbox_ctrl;
	int chan_id;
	int msg_idx;

	if (mbox_chk_valid_drv_id(drv_id)) {
		dev_err(dev, "Invalid driver ID!\n");
		return;
	}

	chan_id = mbox_chan_id_map(dev, drv_id);
	dev_dbg(dev, "%s: drv_id = %d, chan_id = %d\n",
			__func__, drv_id, chan_id);

	mbox_priv_data = dev_get_drvdata(dev);
	mbox_ctrl = mbox_priv_data->mbox_ctrl;
	if (chan_id >= 0 && chan_id < mbox_ctrl->num_chans) {
		mbox_rx_msg = mbox_priv_data->rx_msg;
		dev_dbg(dev, "%s: add chan_id %d msg to buffer\n",
				__func__, chan_id);
		msg_idx = mbox_add_rx_buf(dev, chan_id, aml_data);
		if (msg_idx < 0) {
			dev_err(dev, "Try increasing mailbox rx queue length\n");
			return;
		} else {
			/* Wake up rx thread to process mssg */
			up(&mbox_rx_msg->sem);
		}
	} else {
		dev_err(dev, "%s: error: chan_id %d invalid!\n", __func__, chan_id);
	}
}

static void mbox_isr_handler(struct aml_mbox_chan *aml_chan)
{
	union mbox_stat mbox_stat;
	struct mbox_cmd_t *mbox_cmd = &mbox_stat.mbox_cmd_t;
	struct aml_mbox_data aml_data;
	struct mbox_header *mbox_header;
	struct mbox_data mbox_data;
	struct mbox_controller *mbox_contr = aml_chan->mbox;
	struct device *dev = mbox_contr->dev;
	int drv_id;

	mbox_stat.set_cmd = readl(aml_chan->mbox_fsts_addr);
	dev_dbg(dev, "%s: received cmd 0x%x\n", __func__, mbox_stat.set_cmd);
	if (mbox_stat.set_cmd) {
		if (mbox_cmd->size >= MBOX_HEAD_SIZE) {
			aml_data.cmd = mbox_cmd->cmd;
			if (mbox_cmd->size > (MBOX_DATA_SIZE + MBOX_HEAD_SIZE)) {
				dev_err(dev, "%s: rxsize %x > %x\n",
					__func__, mbox_cmd->size,
					MBOX_DATA_SIZE + MBOX_HEAD_SIZE);
				mbox_cmd->size = MBOX_DATA_SIZE + MBOX_HEAD_SIZE;
			}
			mbox_fifo_read(&mbox_data, aml_chan->mbox_rd_addr, mbox_cmd->size);
			mbox_header = &mbox_data.mbox_header;
			if (mbox_header->complete == MBOX_NO_FEEDBACK)
				goto mbox_isr_handler_done;

			aml_data.rxsize = mbox_cmd->size - MBOX_HEAD_SIZE;
			aml_data.rxbuf = mbox_data.data;
			if (mbox_header->complete != 0) {
				aml_data.rev_complete = mbox_header->complete;
				mbox_wakeup_wait_task(&aml_data);
				goto mbox_isr_handler_done;
			}

			drv_id = mbox_header->status;
			mbox_add_rx_queue(dev, drv_id, &aml_data);
		}
	}
mbox_isr_handler_done:
	mbox_irq_clean(aml_chan, 0);
	writel(~0, aml_chan->mbox_fclr_addr);
}

static void mbox_ack_isr_handler(struct mbox_chan *mbox_chan)
{
	struct aml_mbox_chan *aml_chan = mbox_chan->con_priv;
	struct aml_mbox_data *aml_data = mbox_chan->active_req;

	if (aml_data && aml_data->sync == MBOX_SYNC) {
		if (aml_data->rxbuf && aml_data->rxsize) {
			if (aml_data->rxsize > MBOX_DATA_SIZE) {
				dev_err(mbox_chan->cl->dev, "%s: rxsize %x > %x\n",
					__func__, aml_data->txsize, MBOX_DATA_SIZE);
				aml_data->rxsize = MBOX_DATA_SIZE;
			}
			mbox_fifo_read(aml_data->rxbuf,
				       aml_chan->mbox_rd_addr + MBOX_HEAD_SIZE,
				       aml_data->rxsize);
		}
	}
	aml_chan->tx_complete = NULL;
	mbox_fifo_clr(aml_chan->mbox_wr_addr, MBOX_FIFO_SIZE);
	mbox_irq_clean(aml_chan, 1);
	mbox_chan_txdone(mbox_chan, 0);
}

static u64 mbox_irqstatus(struct aml_mbox_chan *aml_chan)
{
	u64 sts, h_sts, l_sts;

	if (aml_chan->irq_nums / MHUIRQ_MAXNUM_DEF == 2) {
		l_sts = readl(aml_chan->mbox_irqsts_addr);
		h_sts = readl(aml_chan->mbox_irqsts_addr + 4);
		sts = (h_sts << MBOX_IRQSHIFT) | (l_sts & MBOX_IRQMASK);
	} else {
		sts = readl(aml_chan->mbox_irqsts_addr);
	}
	pr_debug("%s irq %llx\n", __func__, sts);
	return sts;
}

static irqreturn_t mbox_handler(int irq, void *p)
{
	struct mbox_controller *mbox_cons = p;
	struct mbox_chan *mbox_chan = &mbox_cons->chans[0];
	struct aml_mbox_chan *aml_chan = mbox_chan->con_priv;
	u64 irq_sts = mbox_irqstatus(aml_chan);
	int out_cnt = mbox_cons->num_chans;
	u64 irq_prests;
	int idx;

	while (irq_sts && (out_cnt != 0)) {
		irq_prests = irq_sts;
		for (idx = 0; idx < mbox_cons->num_chans; idx++) {
			mbox_chan = &mbox_cons->chans[idx];
			aml_chan = mbox_chan->con_priv;
			if (irq_sts & (BIT_ULL(aml_chan->mboxid * 2 + 1))) {
				irq_sts ^= BIT_ULL(aml_chan->mboxid * 2 + 1);
				mbox_chan = container_of(aml_chan->tx_complete,
							 struct mbox_chan, tx_complete);
				if (IS_ERR(mbox_chan)) {
					mbox_irq_clean(aml_chan, 1);
					continue;
				}
				mbox_ack_isr_handler(mbox_chan);
			} else if (irq_sts & (BIT_ULL(aml_chan->mboxid * 2))) {
				irq_sts ^= BIT_ULL(aml_chan->mboxid * 2);
				mbox_isr_handler(aml_chan);
			}
		}
		irq_sts = mbox_irqstatus(aml_chan);
		irq_sts = (irq_sts | irq_prests) ^ irq_prests;
		out_cnt--;
	}

	pr_debug("%s irq\n", __func__);
	return IRQ_HANDLED;
}

static int mbox_transfer_data(struct mbox_chan *mbox_chan, void *data)
{
	struct aml_mbox_chan *aml_chan = mbox_chan->con_priv;
	struct aml_mbox_data *aml_data = data;
	union mbox_stat mbox_stat;
	struct mbox_cmd_t *mbox_cmd = &mbox_stat.mbox_cmd_t;
	struct mbox_header mbox_header;

	mbox_cmd->cmd = aml_data->cmd;
	if (aml_data->txsize > MBOX_DATA_SIZE) {
		dev_err(mbox_chan->cl->dev, "%s: size %x > %x\n",
			__func__, aml_data->txsize, MBOX_DATA_SIZE);
		return -EINVAL;
	}
	mbox_cmd->size = aml_data->txsize + MBOX_HEAD_SIZE;
	if (aml_data->sync == MBOX_SYNC)
		mbox_cmd->sync = SYNC;
	else
		mbox_cmd->sync = ASYNC;
	aml_chan->tx_complete = &mbox_chan->tx_complete;
	memset(&mbox_header, 0, sizeof(mbox_header));
	if (aml_data->sync == MBOX_TSYNC)
		mbox_header.complete = (unsigned long)(&aml_data->complete);
	else
		mbox_header.complete = MBOX_NO_FEEDBACK;
	mbox_fifo_write(aml_chan->mbox_wr_addr, &mbox_header, MBOX_HEAD_SIZE);

	if (aml_data->txbuf) {
		mbox_fifo_write(aml_chan->mbox_wr_addr + MBOX_HEAD_SIZE,
				aml_data->txbuf, aml_data->txsize);
	}
	writel(mbox_stat.set_cmd, aml_chan->mbox_fset_addr);
	return 0;
}

static int mbox_startup(struct mbox_chan *mbox_chan)
{
	return 0;
}

static void mbox_shutdown(struct mbox_chan *mbox_chan)
{
}

static bool mbox_last_tx_done(struct mbox_chan *mbox_chan)
{
	struct aml_mbox_chan *aml_chan = mbox_chan->con_priv;

	return !readl(aml_chan->mbox_fsts_addr);
}

static struct mbox_chan_ops mbox_ops = {
	.send_data = mbox_transfer_data,
	.startup = mbox_startup,
	.shutdown = mbox_shutdown,
	.last_tx_done = mbox_last_tx_done,
};

static int mbox_fifo_parse_dt(struct platform_device *pdev, struct aml_chan_priv *priv)
{
	void __iomem *mbox_wr_base;
	void __iomem *mbox_rd_base;
	void __iomem *mbox_fset_base;
	void __iomem *mbox_fclr_base;
	void __iomem *mbox_fsts_base;
	void __iomem *mbox_irq_base;
	struct aml_mbox_chan *aml_chan;
	struct device *dev = &pdev->dev;
	unsigned int memidx = 0;
	u32 irqctlr = 0;
	u32 irqclr = 0;
	u32 irq_nums = 0;
	u32 mbox_nums = 0;
	u32 mboxid = 0;
	u32 spt_ao_alive_det = 0;
	u32 ao_sts_mboxid = 0;
	u32 ree2ao_mboxid = 0;
	u32 rx_queue_len = 0;
	int idx = 0;
	int err = 0;

	mbox_wr_base = devm_platform_ioremap_resource(pdev, memidx++);
	if (IS_ERR_OR_NULL(mbox_wr_base))
		return PTR_ERR(mbox_wr_base);

	if (of_property_read_bool(dev->of_node, "mbox-wr-same")) {
		mbox_rd_base = mbox_wr_base;
	} else {
		mbox_rd_base = devm_platform_ioremap_resource(pdev, memidx++);
		if (IS_ERR_OR_NULL(mbox_rd_base))
			return PTR_ERR(mbox_rd_base);
	}
	mbox_fset_base = devm_platform_ioremap_resource(pdev, memidx++);
	if (IS_ERR(mbox_fset_base))
		return PTR_ERR(mbox_fset_base);

	mbox_fclr_base = devm_platform_ioremap_resource(pdev, memidx++);
	if (IS_ERR(mbox_fclr_base))
		return PTR_ERR(mbox_fclr_base);

	mbox_fsts_base = devm_platform_ioremap_resource(pdev, memidx++);
	if (IS_ERR(mbox_fsts_base))
		return PTR_ERR(mbox_fsts_base);

	mbox_irq_base = devm_platform_ioremap_resource(pdev, memidx++);
	if (IS_ERR(mbox_irq_base))
		return PTR_ERR(mbox_irq_base);

	err = of_property_read_u32(dev->of_node,
				   "mbox-irqctlr", &irqctlr);
	if (err) {
		dev_dbg(dev, "failed to get mbox irq ctlr %d\n", err);
		return -ENXIO;
	}

	err = of_property_read_u32(dev->of_node,
				   "mbox-irqclr", &irqclr);
	if (err)
		irqclr = irqctlr;

	err = of_property_read_u32(dev->of_node,
				   "mbox-irqnums", &irq_nums);
	if (err) {
		dev_dbg(dev, "set mbox irq_nums to default value\n");
		irq_nums = MHUIRQ_MAXNUM_DEF;
	}

	err = of_property_read_u32(dev->of_node,
				   "mbox-nums", &mbox_nums);
	if (err) {
		dev_dbg(dev, "failed to get mbox num %d\n", err);
		return -ENXIO;
	}
	priv->mbox_nums = mbox_nums;

	aml_chan = devm_kzalloc(dev, sizeof(*aml_chan) * mbox_nums, GFP_KERNEL);
	if (IS_ERR(aml_chan))
		return PTR_ERR(aml_chan);

	err = of_property_read_u32(dev->of_node,
			"aocpu_sts_mboxid", &ao_sts_mboxid);
	if (err) {
		dev_dbg(dev, "Do not support aocpu alive detection\n");
	} else {
		err = of_property_read_u32(dev->of_node,
				"ree2aocpu_mboxid", &ree2ao_mboxid);
		if (err)
			dev_dbg(dev, "failed to get ree2aocpu mbox id, %d\n", err);
		else
			spt_ao_alive_det = 1;
	}

	for (idx = 0; idx < mbox_nums; idx++) {
		err = of_property_read_u32_index(dev->of_node, "mboxids",
						 idx, &mboxid);
		if (err)
			return err;
		aml_chan[idx].mboxid = mboxid;
		aml_chan[idx].irq_nums = irq_nums;
		aml_chan[idx].mbox_wr_addr = mbox_wr_base + PAYLOAD_OFFSET(mboxid);
		aml_chan[idx].mbox_rd_addr = mbox_rd_base + PAYLOAD_OFFSET(mboxid);
		aml_chan[idx].mbox_fset_addr = mbox_fset_base + CTL_OFFSET(mboxid);
		aml_chan[idx].mbox_fclr_addr = mbox_fclr_base + CTL_OFFSET(mboxid);
		aml_chan[idx].mbox_fsts_addr = mbox_fsts_base + CTL_OFFSET(mboxid);
		aml_chan[idx].mbox_irqsts_addr = mbox_irq_base + IRQ_STS_OFFSET(irqctlr);
		aml_chan[idx].mbox_irqclr_addr = mbox_irq_base + IRQ_CLR_OFFSET(irqclr);
		if (spt_ao_alive_det && mboxid == ree2ao_mboxid)
			aml_chan[idx].aocpu_tick_cnt_addr = mbox_rd_base +
				PAYLOAD_OFFSET(ao_sts_mboxid);
		mutex_init(&aml_chan[idx].mutex);
		aml_chan[idx].tx_complete = NULL;
	}
	priv->aml_chan = aml_chan;

	err = of_property_read_u32(dev->of_node,
				   "mbox-rx-queue-length", &rx_queue_len);
	if (err) {
		dev_dbg(dev, "get rx queue length fail, set to default length %d\n",
				MBOX_RX_QUEUE_LEN_DEFAULT);
		rx_queue_len = MBOX_RX_QUEUE_LEN_DEFAULT;
	} else {
		if (rx_queue_len < 1 || rx_queue_len > 100) {
			dev_dbg(dev, "rx queue length should be: 1 < length <= 100\n");
			dev_dbg(dev, "set to default length %d\n", MBOX_RX_QUEUE_LEN_DEFAULT);
			rx_queue_len = MBOX_RX_QUEUE_LEN_DEFAULT;
		}
	}
	dev_dbg(dev, "rx queue length is %d\n", rx_queue_len);
	priv->mbox_rx_queue_len = rx_queue_len;
	return 0;
}

static int mbox_fifo_probe(struct platform_device *pdev)
{
	struct mbox_chan *mbox_chans;
	struct device *dev = &pdev->dev;
	struct aml_mbox_chan *aml_chan;
	struct mbox_controller *mbox_cons;
	const struct mbox_domain_data *match;
	const struct mbox_domain *mbox_domains;
	struct aml_chan_priv aml_priv;
	struct aml_mbox_rx_msg *mbox_rx_msg;
	struct aml_priv_data *mbox_priv_data;
	u32 mboxid = 0;
	u32 mbox_nums;
	u32 rx_pkt_size;
	u32 rx_queue_len;
	int mbox_irq;
	int err = 0;
	int idx0 = 0;
	int idx1 = 0;

	dev_dbg(dev, "mbox fifo probe\n");

	err = mbox_fifo_parse_dt(pdev, &aml_priv);
	if (err) {
		dev_dbg(dev, "mbox parse dt fail\n");
		return err;
	}
	aml_chan = aml_priv.aml_chan;
	mbox_nums = aml_priv.mbox_nums;
	rx_queue_len = aml_priv.mbox_rx_queue_len;

	match = of_device_get_match_data(&pdev->dev);
	if (!match) {
		dev_dbg(&pdev->dev, "failed to get match data\n");
		return -ENODEV;
	}
	mbox_domains = match->mbox_domains;
	mbox_chans = devm_kzalloc(dev, sizeof(*mbox_chans) * match->domain_counts, GFP_KERNEL);
	if (IS_ERR(mbox_chans))
		return PTR_ERR(mbox_chans);

	for (idx0 = 0; idx0 < match->domain_counts; idx0++) {
		mboxid = mbox_domains[idx0].mboxid;
		for (idx1 = 0; idx1 < mbox_nums; idx1++) {
			if (mboxid == aml_chan[idx1].mboxid) {
				mbox_chans[idx0].con_priv = &aml_chan[idx1];
				break;
			}
		}
	}
	mbox_cons = devm_kzalloc(dev, sizeof(*mbox_cons), GFP_KERNEL);
	if (IS_ERR(mbox_cons))
		return PTR_ERR(mbox_cons);

	mbox_cons->of_xlate = mbox_chan_xlate;
	mbox_cons->chans = mbox_chans;
	mbox_cons->num_chans = match->domain_counts;
	mbox_cons->txdone_irq = true;
	mbox_cons->ops = &mbox_ops;
	mbox_cons->dev = dev;

	mbox_priv_data = devm_kzalloc(dev, sizeof(*mbox_priv_data), GFP_KERNEL);
	if (IS_ERR(mbox_priv_data))
		return PTR_ERR(mbox_priv_data);
	mbox_priv_data->domain_data.mbox_domains = match->mbox_domains;
	mbox_priv_data->domain_data.domain_counts = match->domain_counts;
	mbox_priv_data->mbox_ctrl = mbox_cons;

	mbox_rx_msg = devm_kzalloc(dev, sizeof(*mbox_rx_msg), GFP_KERNEL);
	if (IS_ERR(mbox_rx_msg))
		return PTR_ERR(mbox_rx_msg);

	rx_pkt_size = sizeof(struct aml_mbox_rx_packet);
	mbox_rx_msg->msg_data = devm_kzalloc(dev,
			rx_pkt_size * rx_queue_len, GFP_KERNEL);
	if (IS_ERR(mbox_rx_msg->msg_data))
		return PTR_ERR(mbox_rx_msg->msg_data);
	mbox_rx_msg->msg_count = 0;
	mbox_rx_msg->msg_free = 0;
	mbox_rx_msg->rx_queue_len = rx_queue_len;
	spin_lock_init(&mbox_rx_msg->lock);
	sema_init(&mbox_rx_msg->sem, 0);
	mbox_priv_data->rx_msg = mbox_rx_msg;

	platform_set_drvdata(pdev, mbox_priv_data);
	err = devm_mbox_controller_register(dev, mbox_cons);
	if (err) {
		dev_err(dev, "failed to register mailbox controller\n");
		return err;
	}

	mbox_rx_msg->thread =
		kthread_run(mbox_rx_process, dev, "mbox_rx_thread");
	if (IS_ERR_OR_NULL(mbox_rx_msg->thread)) {
		dev_dbg(dev, "Failed to create rx thread\n");
		return PTR_ERR(mbox_rx_msg->thread);
	}

	for (idx0 = 0; idx0 < mbox_nums; idx0++)
		aml_chan[idx0].mbox = mbox_cons;

	mbox_irq = platform_get_irq(pdev, 0);
	if (mbox_irq < 0) {
		dev_dbg(dev, "failed to get interrupt %d\n", mbox_irq);
		return -ENXIO;
	}

#ifndef CONFIG_PREEMPT_RT
	err = request_threaded_irq(mbox_irq, mbox_handler,
				   NULL, IRQF_ONESHOT | IRQF_NO_SUSPEND,
				   DRIVER_NAME, mbox_cons);
#else
	err = request_threaded_irq(mbox_irq, NULL,
				   mbox_handler, IRQF_ONESHOT | IRQF_NO_SUSPEND,
				   DRIVER_NAME, mbox_cons);
#endif
	if (err) {
		dev_dbg(dev, "request irq error\n");
		return err;
	}

	dev_dbg(dev, "mbox fifo probe done\n");
	return 0;
}

static void mbox_fifo_remove(struct platform_device *pdev)
{
	struct aml_priv_data *mbox_priv_data;
	struct aml_mbox_rx_msg *mbox_rx_msg;

	mbox_priv_data = platform_get_drvdata(pdev);
	mbox_rx_msg = mbox_priv_data->rx_msg;

	if (mbox_rx_msg->thread) {
		up(&mbox_rx_msg->sem);
		kthread_stop(mbox_rx_msg->thread);
	}
	platform_set_drvdata(pdev, NULL);
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
const struct mbox_domain sc2_mbox_domains[] = {
	MBOX_DOMAIN(SC2_AO2REE0, SC2_MBOX_AO2REE, 0),
	MBOX_DOMAIN(SC2_AO2REE1, SC2_MBOX_AO2REE, 0),
	MBOX_DOMAIN(SC2_AO2REE2, SC2_MBOX_AO2REE, 0),
	MBOX_DOMAIN(SC2_AO2REE3, SC2_MBOX_AO2REE, 0),
	MBOX_DOMAIN(SC2_AO2REE4, SC2_MBOX_AO2REE, 0),
	MBOX_DOMAIN(SC2_AO2REE5, SC2_MBOX_AO2REE, 0),
	MBOX_DOMAIN(SC2_AO2REE6, SC2_MBOX_AO2REE, 0),
	MBOX_DOMAIN(SC2_AO2REE7, SC2_MBOX_AO2REE, 0),
	MBOX_DOMAIN(SC2_AO2REE8, SC2_MBOX_AO2REE, 0),
	MBOX_DOMAIN(SC2_AO2REE9, SC2_MBOX_AO2REE, 0),
	MBOX_DOMAIN(SC2_AO2REE10, SC2_MBOX_AO2REE, 0),
	MBOX_DOMAIN(SC2_REE2AO0, SC2_MBOX_REE2AO, 0),
	MBOX_DOMAIN(SC2_REE2AO1, SC2_MBOX_REE2AO, 0),
	MBOX_DOMAIN(SC2_REE2AO2, SC2_MBOX_REE2AO, 0),
	MBOX_DOMAIN(SC2_REE2AO3, SC2_MBOX_REE2AO, 0),
	MBOX_DOMAIN(SC2_REE2AO4, SC2_MBOX_REE2AO, 0),
	MBOX_DOMAIN(SC2_REE2AO5, SC2_MBOX_REE2AO, 0),
	MBOX_DOMAIN(SC2_REE2AO6, SC2_MBOX_REE2AO, 0),
	MBOX_DOMAIN(SC2_REE2AO7, SC2_MBOX_REE2AO, 0),
	MBOX_DOMAIN(SC2_REE2AO8, SC2_MBOX_REE2AO, 0),
	MBOX_DOMAIN(SC2_REE2AO9, SC2_MBOX_REE2AO, 0),
	MBOX_DOMAIN(SC2_REE2AO10, SC2_MBOX_REE2AO, 0),
	MBOX_DOMAIN(SC2_DSPA2REE0, SC2_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(SC2_DSPA2REE1, SC2_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(SC2_DSPA2REE2, SC2_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(SC2_DSPA2REE3, SC2_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(SC2_DSPA2REE4, SC2_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(SC2_REE2DSPA0, SC2_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(SC2_REE2DSPA1, SC2_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(SC2_REE2DSPA2, SC2_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(SC2_REE2DSPA3, SC2_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(SC2_REE2DSPA4, SC2_MBOX_REE2DSPA, 0),
};

static struct mbox_domain_data sc2_mbox_domains_data __initdata = {
	.mbox_domains = sc2_mbox_domains,
	.domain_counts = ARRAY_SIZE(sc2_mbox_domains),
};

const struct mbox_domain t3_mbox_domains[] = {
	MBOX_DOMAIN(T3_AO2REE0, T3_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T3_AO2REE1, T3_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T3_AO2REE2, T3_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T3_AO2REE3, T3_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T3_AO2REE4, T3_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T3_AO2REE5, T3_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T3_AO2REE6, T3_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T3_AO2REE7, T3_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T3_AO2REE8, T3_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T3_AO2REE9, T3_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T3_AO2REE10, T3_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T3_REE2AO0, T3_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T3_REE2AO1, T3_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T3_REE2AO2, T3_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T3_REE2AO3, T3_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T3_REE2AO4, T3_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T3_REE2AO5, T3_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T3_REE2AO6, T3_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T3_REE2AO7, T3_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T3_REE2AO8, T3_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T3_REE2AO9, T3_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T3_REE2AO10, T3_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T3_DSPA2REE0, T3_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(T3_DSPA2REE1, T3_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(T3_DSPA2REE2, T3_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(T3_DSPA2REE3, T3_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(T3_DSPA2REE4, T3_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(T3_REE2DSPA0, T3_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(T3_REE2DSPA1, T3_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(T3_REE2DSPA2, T3_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(T3_REE2DSPA3, T3_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(T3_REE2DSPA4, T3_MBOX_REE2DSPA, 0),
};

static struct mbox_domain_data t3_mbox_domains_data __initdata = {
	.mbox_domains = t3_mbox_domains,
	.domain_counts = ARRAY_SIZE(t3_mbox_domains),
};

const struct mbox_domain t7_mbox_domains[] = {
	MBOX_DOMAIN(T7_AO2REE0, T7_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T7_AO2REE1, T7_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T7_AO2REE2, T7_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T7_AO2REE3, T7_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T7_AO2REE4, T7_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T7_AO2REE5, T7_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T7_AO2REE6, T7_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T7_AO2REE7, T7_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T7_AO2REE8, T7_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T7_AO2REE9, T7_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T7_AO2REE10, T7_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T7_REE2AO0, T7_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T7_REE2AO1, T7_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T7_REE2AO2, T7_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T7_REE2AO3, T7_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T7_REE2AO4, T7_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T7_REE2AO5, T7_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T7_REE2AO6, T7_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T7_REE2AO7, T7_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T7_REE2AO8, T7_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T7_REE2AO9, T7_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T7_REE2AO10, T7_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T7_DSPA2REE0, T7_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(T7_DSPA2REE1, T7_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(T7_DSPA2REE2, T7_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(T7_DSPA2REE3, T7_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(T7_DSPA2REE4, T7_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(T7_REE2DSPA0, T7_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(T7_REE2DSPA1, T7_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(T7_REE2DSPA2, T7_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(T7_REE2DSPA3, T7_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(T7_REE2DSPA4, T7_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(T7_DSPB2REE0, T7_MBOX_DSPB2REE, 0),
	MBOX_DOMAIN(T7_DSPB2REE1, T7_MBOX_DSPB2REE, 0),
	MBOX_DOMAIN(T7_DSPB2REE2, T7_MBOX_DSPB2REE, 0),
	MBOX_DOMAIN(T7_DSPB2REE3, T7_MBOX_DSPB2REE, 0),
	MBOX_DOMAIN(T7_DSPB2REE4, T7_MBOX_DSPB2REE, 0),
	MBOX_DOMAIN(T7_REE2DSPB0, T7_MBOX_REE2DSPB, 0),
	MBOX_DOMAIN(T7_REE2DSPB1, T7_MBOX_REE2DSPB, 0),
	MBOX_DOMAIN(T7_REE2DSPB2, T7_MBOX_REE2DSPB, 0),
	MBOX_DOMAIN(T7_REE2DSPB3, T7_MBOX_REE2DSPB, 0),
	MBOX_DOMAIN(T7_REE2DSPB4, T7_MBOX_REE2DSPB, 0),
};

static struct mbox_domain_data t7_mbox_domains_data __initdata = {
	.mbox_domains = t7_mbox_domains,
	.domain_counts = ARRAY_SIZE(t7_mbox_domains),
};

const struct mbox_domain s4_mbox_domains[] = {
	MBOX_DOMAIN(S4_AO2REE0, S4_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S4_AO2REE1, S4_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S4_AO2REE2, S4_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S4_AO2REE3, S4_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S4_AO2REE4, S4_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S4_AO2REE5, S4_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S4_AO2REE6, S4_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S4_AO2REE7, S4_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S4_AO2REE8, S4_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S4_AO2REE9, S4_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S4_AO2REE10, S4_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S4_REE2AO0, S4_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S4_REE2AO1, S4_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S4_REE2AO2, S4_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S4_REE2AO3, S4_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S4_REE2AO4, S4_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S4_REE2AO5, S4_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S4_REE2AO6, S4_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S4_REE2AO7, S4_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S4_REE2AO8, S4_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S4_REE2AO9, S4_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S4_REE2AO10, S4_MBOX_REE2AO, 0),
};

static struct mbox_domain_data s4_mbox_domains_data __initdata = {
	.mbox_domains = s4_mbox_domains,
	.domain_counts = ARRAY_SIZE(s4_mbox_domains),
};

const struct mbox_domain t5m_mbox_domains[] = {
	MBOX_DOMAIN(T5M_AO2REE0, T5M_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T5M_AO2REE1, T5M_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T5M_AO2REE2, T5M_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T5M_AO2REE3, T5M_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T5M_AO2REE4, T5M_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T5M_AO2REE5, T5M_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T5M_AO2REE6, T5M_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T5M_AO2REE7, T5M_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T5M_AO2REE8, T5M_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T5M_AO2REE9, T5M_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T5M_AO2REE10, T5M_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T5M_REE2AO0, T5M_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T5M_REE2AO1, T5M_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T5M_REE2AO2, T5M_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T5M_REE2AO3, T5M_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T5M_REE2AO4, T5M_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T5M_REE2AO5, T5M_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T5M_REE2AO6, T5M_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T5M_REE2AO7, T5M_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T5M_REE2AO8, T5M_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T5M_REE2AO9, T5M_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T5M_REE2AO10, T5M_MBOX_REE2AO, 0),
};

static struct mbox_domain_data t5m_mbox_domains_data __initdata = {
	.mbox_domains = t5m_mbox_domains,
	.domain_counts = ARRAY_SIZE(t5m_mbox_domains),
};

const struct mbox_domain t3x_mbox_domains[] = {
	MBOX_DOMAIN(T3X_AO2REE0, T3X_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T3X_AO2REE1, T3X_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T3X_AO2REE2, T3X_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T3X_AO2REE3, T3X_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T3X_AO2REE4, T3X_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T3X_AO2REE5, T3X_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T3X_AO2REE6, T3X_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T3X_AO2REE7, T3X_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T3X_AO2REE8, T3X_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T3X_AO2REE9, T3X_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T3X_AO2REE10, T3X_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T3X_REE2AO0, T3X_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T3X_REE2AO1, T3X_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T3X_REE2AO2, T3X_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T3X_REE2AO3, T3X_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T3X_REE2AO4, T3X_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T3X_REE2AO5, T3X_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T3X_REE2AO6, T3X_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T3X_REE2AO7, T3X_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T3X_REE2AO8, T3X_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T3X_REE2AO9, T3X_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T3X_REE2AO10, T3X_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T3X_DSPA2REE0, T3X_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(T3X_DSPA2REE1, T3X_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(T3X_DSPA2REE2, T3X_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(T3X_DSPA2REE3, T3X_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(T3X_DSPA2REE4, T3X_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(T3X_REE2DSPA0, T3X_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(T3X_REE2DSPA1, T3X_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(T3X_REE2DSPA2, T3X_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(T3X_REE2DSPA3, T3X_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(T3X_REE2DSPA4, T3X_MBOX_REE2DSPA, 0),
};

static struct mbox_domain_data t3x_mbox_domains_data __initdata = {
	.mbox_domains = t3x_mbox_domains,
	.domain_counts = ARRAY_SIZE(t3x_mbox_domains),
};

const struct mbox_domain s5_mbox_domains[] = {
	MBOX_DOMAIN(S5_AO2REE0, S5_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S5_AO2REE1, S5_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S5_AO2REE2, S5_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S5_AO2REE3, S5_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S5_AO2REE4, S5_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S5_AO2REE5, S5_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S5_AO2REE6, S5_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S5_AO2REE7, S5_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S5_AO2REE8, S5_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S5_AO2REE9, S5_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S5_AO2REE10, S5_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S5_REE2AO0, S5_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S5_REE2AO1, S5_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S5_REE2AO2, S5_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S5_REE2AO3, S5_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S5_REE2AO4, S5_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S5_REE2AO5, S5_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S5_REE2AO6, S5_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S5_REE2AO7, S5_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S5_REE2AO8, S5_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S5_REE2AO9, S5_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S5_REE2AO10, S5_MBOX_REE2AO, 0),
};

static struct mbox_domain_data s5_mbox_domains_data __initdata = {
	.mbox_domains = s5_mbox_domains,
	.domain_counts = ARRAY_SIZE(s5_mbox_domains),
};

const struct mbox_domain s7_mbox_domains[] = {
	MBOX_DOMAIN(S7_AO2REE0, S7_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S7_AO2REE1, S7_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S7_AO2REE2, S7_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S7_AO2REE3, S7_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S7_AO2REE4, S7_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S7_AO2REE5, S7_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S7_AO2REE6, S7_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S7_AO2REE7, S7_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S7_AO2REE8, S7_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S7_AO2REE9, S7_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S7_AO2REE10, S7_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S7_REE2AO0, S7_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S7_REE2AO1, S7_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S7_REE2AO2, S7_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S7_REE2AO3, S7_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S7_REE2AO4, S7_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S7_REE2AO5, S7_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S7_REE2AO6, S7_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S7_REE2AO7, S7_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S7_REE2AO8, S7_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S7_REE2AO9, S7_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S7_REE2AO10, S7_MBOX_REE2AO, 0),
};

static struct mbox_domain_data s7_mbox_domains_data __initdata = {
	.mbox_domains = s7_mbox_domains,
	.domain_counts = ARRAY_SIZE(s7_mbox_domains),
};

const struct mbox_domain s6_mbox_domains[] = {
	MBOX_DOMAIN(S6_AO2REE0, S6_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S6_AO2REE1, S6_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S6_AO2REE2, S6_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S6_AO2REE3, S6_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S6_AO2REE4, S6_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S6_AO2REE5, S6_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S6_AO2REE6, S6_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S6_AO2REE7, S6_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S6_AO2REE8, S6_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S6_AO2REE9, S6_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S6_AO2REE10, S6_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S6_REE2AO0, S6_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S6_REE2AO1, S6_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S6_REE2AO2, S6_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S6_REE2AO3, S6_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S6_REE2AO4, S6_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S6_REE2AO5, S6_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S6_REE2AO6, S6_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S6_REE2AO7, S6_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S6_REE2AO8, S6_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S6_REE2AO9, S6_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S6_REE2AO10, S6_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S6_DSPA2REE0, S6_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(S6_DSPA2REE1, S6_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(S6_DSPA2REE2, S6_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(S6_DSPA2REE3, S6_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(S6_DSPA2REE4, S6_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(S6_REE2DSPA0, S6_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(S6_REE2DSPA1, S6_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(S6_REE2DSPA2, S6_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(S6_REE2DSPA3, S6_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(S6_REE2DSPA4, S6_MBOX_REE2DSPA, 0),
};

static struct mbox_domain_data s6_mbox_domains_data __initdata = {
	.mbox_domains = s6_mbox_domains,
	.domain_counts = ARRAY_SIZE(s6_mbox_domains),
};

const struct mbox_domain a4_mbox_domains[] = {
	MBOX_DOMAIN(A4_AO2REE0, A4_MBOX_AO2REE, 0),
	MBOX_DOMAIN(A4_AO2REE1, A4_MBOX_AO2REE, 0),
	MBOX_DOMAIN(A4_AO2REE2, A4_MBOX_AO2REE, 0),
	MBOX_DOMAIN(A4_AO2REE3, A4_MBOX_AO2REE, 0),
	MBOX_DOMAIN(A4_AO2REE4, A4_MBOX_AO2REE, 0),
	MBOX_DOMAIN(A4_AO2REE5, A4_MBOX_AO2REE, 0),
	MBOX_DOMAIN(A4_AO2REE6, A4_MBOX_AO2REE, 0),
	MBOX_DOMAIN(A4_AO2REE7, A4_MBOX_AO2REE, 0),
	MBOX_DOMAIN(A4_AO2REE8, A4_MBOX_AO2REE, 0),
	MBOX_DOMAIN(A4_AO2REE9, A4_MBOX_AO2REE, 0),
	MBOX_DOMAIN(A4_AO2REE10, A4_MBOX_AO2REE, 0),
	MBOX_DOMAIN(A4_REE2AO0, A4_MBOX_REE2AO, 0),
	MBOX_DOMAIN(A4_REE2AO1, A4_MBOX_REE2AO, 0),
	MBOX_DOMAIN(A4_REE2AO2, A4_MBOX_REE2AO, 0),
	MBOX_DOMAIN(A4_REE2AO3, A4_MBOX_REE2AO, 0),
	MBOX_DOMAIN(A4_REE2AO4, A4_MBOX_REE2AO, 0),
	MBOX_DOMAIN(A4_REE2AO5, A4_MBOX_REE2AO, 0),
	MBOX_DOMAIN(A4_REE2AO6, A4_MBOX_REE2AO, 0),
	MBOX_DOMAIN(A4_REE2AO7, A4_MBOX_REE2AO, 0),
	MBOX_DOMAIN(A4_REE2AO8, A4_MBOX_REE2AO, 0),
	MBOX_DOMAIN(A4_REE2AO9, A4_MBOX_REE2AO, 0),
	MBOX_DOMAIN(A4_REE2AO10, A4_MBOX_REE2AO, 0),
};

static struct mbox_domain_data a4_mbox_domains_data __initdata = {
	.mbox_domains = a4_mbox_domains,
	.domain_counts = ARRAY_SIZE(a4_mbox_domains),
};

const struct mbox_domain a5_mbox_domains[] = {
	MBOX_DOMAIN(A5_AO2REE0, A5_MBOX_AO2REE, 0),
	MBOX_DOMAIN(A5_AO2REE1, A5_MBOX_AO2REE, 0),
	MBOX_DOMAIN(A5_AO2REE2, A5_MBOX_AO2REE, 0),
	MBOX_DOMAIN(A5_AO2REE3, A5_MBOX_AO2REE, 0),
	MBOX_DOMAIN(A5_AO2REE4, A5_MBOX_AO2REE, 0),
	MBOX_DOMAIN(A5_AO2REE5, A5_MBOX_AO2REE, 0),
	MBOX_DOMAIN(A5_AO2REE6, A5_MBOX_AO2REE, 0),
	MBOX_DOMAIN(A5_AO2REE7, A5_MBOX_AO2REE, 0),
	MBOX_DOMAIN(A5_AO2REE8, A5_MBOX_AO2REE, 0),
	MBOX_DOMAIN(A5_AO2REE9, A5_MBOX_AO2REE, 0),
	MBOX_DOMAIN(A5_AO2REE10, A5_MBOX_AO2REE, 0),
	MBOX_DOMAIN(A5_REE2AO0, A5_MBOX_REE2AO, 0),
	MBOX_DOMAIN(A5_REE2AO1, A5_MBOX_REE2AO, 0),
	MBOX_DOMAIN(A5_REE2AO2, A5_MBOX_REE2AO, 0),
	MBOX_DOMAIN(A5_REE2AO3, A5_MBOX_REE2AO, 0),
	MBOX_DOMAIN(A5_REE2AO4, A5_MBOX_REE2AO, 0),
	MBOX_DOMAIN(A5_REE2AO5, A5_MBOX_REE2AO, 0),
	MBOX_DOMAIN(A5_REE2AO6, A5_MBOX_REE2AO, 0),
	MBOX_DOMAIN(A5_REE2AO7, A5_MBOX_REE2AO, 0),
	MBOX_DOMAIN(A5_REE2AO8, A5_MBOX_REE2AO, 0),
	MBOX_DOMAIN(A5_REE2AO9, A5_MBOX_REE2AO, 0),
	MBOX_DOMAIN(A5_REE2AO10, A5_MBOX_REE2AO, 0),
	MBOX_DOMAIN(A5_DSPA2REE0, A5_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(A5_DSPA2REE1, A5_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(A5_DSPA2REE2, A5_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(A5_DSPA2REE3, A5_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(A5_DSPA2REE4, A5_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(A5_REE2DSPA0, A5_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(A5_REE2DSPA1, A5_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(A5_REE2DSPA2, A5_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(A5_REE2DSPA3, A5_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(A5_REE2DSPA4, A5_MBOX_REE2DSPA, 0),
};

static struct mbox_domain_data a5_mbox_domains_data __initdata = {
	.mbox_domains = a5_mbox_domains,
	.domain_counts = ARRAY_SIZE(a5_mbox_domains),
};

const struct mbox_domain t6d_mbox_domains[] = {
	MBOX_DOMAIN(T6D_AO2REE0,  T6D_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T6D_AO2REE1,  T6D_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T6D_AO2REE2,  T6D_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T6D_AO2REE3,  T6D_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T6D_AO2REE4,  T6D_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T6D_AO2REE5,  T6D_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T6D_AO2REE6,  T6D_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T6D_AO2REE7,  T6D_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T6D_AO2REE8,  T6D_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T6D_AO2REE9,  T6D_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T6D_AO2REE10,  T6D_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T6D_REE2AO0, T6D_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6D_REE2AO1, T6D_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6D_REE2AO2, T6D_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6D_REE2AO3, T6D_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6D_REE2AO4, T6D_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6D_REE2AO5, T6D_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6D_REE2AO6, T6D_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6D_REE2AO7, T6D_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6D_REE2AO8, T6D_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6D_REE2AO9, T6D_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6D_REE2AO10, T6D_MBOX_REE2AO, 0),
};

static struct mbox_domain_data t6d_mbox_domains_data __initdata = {
	.mbox_domains = t6d_mbox_domains,
	.domain_counts = ARRAY_SIZE(t6d_mbox_domains),
};

const struct mbox_domain t6w_mbox_domains[] = {
	MBOX_DOMAIN(T6W_REE2AO0, T6W_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6W_REE2AO1, T6W_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6W_REE2AO2, T6W_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6W_REE2AO3, T6W_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6W_REE2AO4, T6W_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6W_REE2AO5, T6W_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6W_REE2AO6, T6W_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6W_REE2AO7, T6W_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6W_REE2AO8, T6W_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6W_REE2AO9, T6W_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6W_REE2AO10, T6W_MBOX_REE2AO, 0),
};

static struct mbox_domain_data t6w_mbox_domains_data __initdata = {
	.mbox_domains = t6w_mbox_domains,
	.domain_counts = ARRAY_SIZE(t6w_mbox_domains),
};

const struct mbox_domain s7d_mbox_domains[] = {
	MBOX_DOMAIN(S7D_AO2REE0, S7D_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S7D_AO2REE1, S7D_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S7D_AO2REE2, S7D_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S7D_AO2REE3, S7D_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S7D_AO2REE4, S7D_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S7D_AO2REE5, S7D_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S7D_AO2REE6, S7D_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S7D_AO2REE7, S7D_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S7D_AO2REE8, S7D_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S7D_AO2REE9, S7D_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S7D_AO2REE10, S7D_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S7D_REE2AO0, S7D_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S7D_REE2AO1, S7D_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S7D_REE2AO2, S7D_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S7D_REE2AO3, S7D_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S7D_REE2AO4, S7D_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S7D_REE2AO5, S7D_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S7D_REE2AO6, S7D_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S7D_REE2AO7, S7D_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S7D_REE2AO8, S7D_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S7D_REE2AO9, S7D_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S7D_REE2AO10, S7D_MBOX_REE2AO, 0),
};

static struct mbox_domain_data s7d_mbox_domains_data __initdata = {
	.mbox_domains = s7d_mbox_domains,
	.domain_counts = ARRAY_SIZE(s7d_mbox_domains),
};

const struct mbox_domain t6x_mbox_domains[] = {
	MBOX_DOMAIN(T6X_AO2REE0, T6X_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T6X_AO2REE1, T6X_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T6X_AO2REE2, T6X_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T6X_AO2REE3, T6X_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T6X_AO2REE4, T6X_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T6X_AO2REE5, T6X_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T6X_AO2REE6, T6X_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T6X_AO2REE7, T6X_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T6X_AO2REE8, T6X_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T6X_AO2REE9, T6X_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T6X_AO2REE10, T6X_MBOX_AO2REE, 0),
	MBOX_DOMAIN(T6X_REE2AO0, T6X_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6X_REE2AO1, T6X_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6X_REE2AO2, T6X_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6X_REE2AO3, T6X_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6X_REE2AO4, T6X_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6X_REE2AO5, T6X_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6X_REE2AO6, T6X_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6X_REE2AO7, T6X_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6X_REE2AO8, T6X_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6X_REE2AO9, T6X_MBOX_REE2AO, 0),
	MBOX_DOMAIN(T6X_REE2AO10, T6X_MBOX_REE2AO, 0),
};

static struct mbox_domain_data t6x_mbox_domains_data __initdata = {
	.mbox_domains = t6x_mbox_domains,
	.domain_counts = ARRAY_SIZE(t6x_mbox_domains),
};
#endif

const struct mbox_domain s1a_mbox_domains[] = {
	MBOX_DOMAIN(S1A_AO2REE0, S1A_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S1A_AO2REE1, S1A_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S1A_AO2REE2, S1A_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S1A_AO2REE3, S1A_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S1A_AO2REE4, S1A_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S1A_AO2REE5, S1A_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S1A_AO2REE6, S1A_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S1A_AO2REE7, S1A_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S1A_AO2REE8, S1A_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S1A_AO2REE9, S1A_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S1A_AO2REE10, S1A_MBOX_AO2REE, 0),
	MBOX_DOMAIN(S1A_REE2AO0, S1A_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S1A_REE2AO1, S1A_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S1A_REE2AO2, S1A_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S1A_REE2AO3, S1A_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S1A_REE2AO4, S1A_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S1A_REE2AO5, S1A_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S1A_REE2AO6, S1A_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S1A_REE2AO7, S1A_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S1A_REE2AO8, S1A_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S1A_REE2AO9, S1A_MBOX_REE2AO, 0),
	MBOX_DOMAIN(S1A_REE2AO10, S1A_MBOX_REE2AO, 0),
};

static struct mbox_domain_data s1a_mbox_domains_data __initdata = {
	.mbox_domains = s1a_mbox_domains,
	.domain_counts = ARRAY_SIZE(s1a_mbox_domains),
};

static const struct of_device_id mbox_of_match[] = {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	{
		.compatible = "amlogic, t3-mbox-fifo",
		.data = &t3_mbox_domains_data,
	},
	{
		.compatible = "amlogic, t7-mbox-fifo",
		.data = &t7_mbox_domains_data,
	},
	{
		.compatible = "amlogic, sc2-mbox-fifo",
		.data = &sc2_mbox_domains_data,
	},
	{
		.compatible = "amlogic, s4-mbox-fifo",
		.data = &s4_mbox_domains_data,
	},
	{
		.compatible = "amlogic, t5m-mbox-fifo",
		.data = &t5m_mbox_domains_data,
	},
	{
		.compatible = "amlogic, t3x-mbox-fifo",
		.data = &t3x_mbox_domains_data,
	},
	{
		.compatible = "amlogic, s5-mbox-fifo",
		.data = &s5_mbox_domains_data,
	},
	{
		.compatible = "amlogic, s7-mbox-fifo",
		.data = &s7_mbox_domains_data,
	},
	{
		.compatible = "amlogic, s6-mbox-fifo",
		.data = &s6_mbox_domains_data,
	},
	{
		.compatible = "amlogic, a4-mbox-fifo",
		.data = &a4_mbox_domains_data,
	},
	{
		.compatible = "amlogic, a5-mbox-fifo",
		.data = &a5_mbox_domains_data,
	},
	{
		.compatible = "amlogic, t6d-mbox-fifo",
		.data = &t6d_mbox_domains_data,
	},
	{
		.compatible = "amlogic, t6w-mbox-fifo",
		.data = &t6w_mbox_domains_data,
	},
	{
		.compatible = "amlogic, s7d-mbox-fifo",
		.data = &s7d_mbox_domains_data,
	},
	{
		.compatible = "amlogic, t6x-mbox-fifo",
		.data = &t6x_mbox_domains_data,
	},
#endif
	{
		.compatible = "amlogic, s1a-mbox-fifo",
		.data = &s1a_mbox_domains_data,
	},
	{},
};

static struct platform_driver mbox_fifo_driver = {
	.probe = mbox_fifo_probe,
	.remove = mbox_fifo_remove,
	.driver = {
		.owner	= THIS_MODULE,
		.name   = DRIVER_NAME,
		.of_match_table = mbox_of_match,
	},
};

int __init mbox_fifo_init(void)
{
	return platform_driver_register(&mbox_fifo_driver);
}

void __exit mbox_fifo_exit(void)
{
	platform_driver_unregister(&mbox_fifo_driver);
}
