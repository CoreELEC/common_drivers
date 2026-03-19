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
#include <linux/of_device.h>
#include <linux/kthread.h>
#include <linux/amlogic/aml_mbox.h>
#include <dt-bindings/mailbox/c2-mbox.h>
#include <dt-bindings/mailbox/a1-mbox.h>
#include <dt-bindings/mailbox/c1-mbox.h>
#include <dt-bindings/mailbox/tm2-mbox.h>
#include <dt-bindings/mailbox/g12a-mbox.h>
#include "meson_mbox_pl.h"
#include "meson_mbox_comm.h"

#define DRIVER_NAME     "meson_mbox_pl"

#define MBOX_SYNC_HEAD_SIZE  0x8
#define MBOX_ASYNC_HEAD_SIZE 0x10
#define MBOX_PAYLOAD_SIZE    0x80
#define MBOX_TRANS_DSP2ARM   0x5A5A1234
#define ASYNC	1
#define SYNC	2

struct mbox_sync_header {
	u64 reserve;
};

struct mbox_drvid_head_info {
	u32 drv_id;
	u32 trans_mode_pattern;
} __packed;

union mbox_comp_head_info {
	u64 complete;
	struct mbox_drvid_head_info drvid_info;
};

struct mbox_async_header {
	u64 reserve;
	union mbox_comp_head_info comp_info;
};

struct mbox_data_async {
	struct mbox_async_header async_header;
	char data[MBOX_DATA_SIZE];
} __packed;

struct mbox_data_sync {
	struct mbox_sync_header sync_header;
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

static int mbox_transfer_data(struct mbox_chan *mbox_chan, void *data)
{
	struct aml_mbox_chan *aml_chan = mbox_chan->con_priv;
	struct aml_mbox_data *aml_data = data;
	union mbox_stat mbox_stat;
	struct mbox_cmd_t *mbox_cmd = &mbox_stat.mbox_cmd_t;
	struct mbox_async_header async_header;
	u32 head_size;

	mbox_cmd->cmd = aml_data->cmd;

	if (!aml_chan->tx_chan) {
		dev_err(mbox_chan->cl->dev, "%s: mbox channel is not tx channel\n",
			__func__);
		return -EINVAL;
	}
	if (aml_data->txsize > MBOX_DATA_SIZE) {
		dev_err(mbox_chan->cl->dev, "%s: size %x > %x\n",
			__func__, aml_data->txsize, MBOX_DATA_SIZE);
		return -EINVAL;
	}

	if (aml_data->sync == MBOX_SYNC) {
		head_size = MBOX_SYNC_HEAD_SIZE;
		mbox_cmd->sync = SYNC;
	} else {
		head_size = MBOX_ASYNC_HEAD_SIZE;
		mbox_cmd->sync = ASYNC;
		if (aml_data->sync == MBOX_TSYNC)
			async_header.comp_info.complete = (unsigned long)(&aml_data->complete);
		else
			async_header.comp_info.complete = MBOX_NO_FEEDBACK;
		mbox_fifo_write(aml_chan->mbox_wr_addr, &async_header, head_size);
	}

	mbox_cmd->size = aml_data->txsize + head_size;
	aml_chan->tx_complete = &mbox_chan->tx_complete;
	if (aml_data->txbuf) {
		mbox_fifo_write(aml_chan->mbox_wr_addr + head_size,
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
	void __iomem *mbox_fsts_addr = aml_chan->mbox_fsts_addr;

	return !readl(mbox_fsts_addr);
}

static struct mbox_chan_ops mbox_pl_ops = {
	.send_data = mbox_transfer_data,
	.startup = mbox_startup,
	.shutdown = mbox_shutdown,
	.last_tx_done = mbox_last_tx_done,
};

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

static irqreturn_t mbox_irq_handler(int irq, void *p)
{
	struct aml_mbox_chan *aml_chan = p;
	struct mbox_controller *mbox_contr = aml_chan->mbox;
	struct mbox_data_async data_async;
	struct mbox_async_header *async_header;
	struct aml_mbox_data aml_data;
	union mbox_stat mbox_stat;
	struct mbox_cmd_t *mbox_cmd = &mbox_stat.mbox_cmd_t;
	struct device *dev = mbox_contr->dev;
	int drv_id;
	u32 trans_mode_pattern;
	u64 complete;

	mbox_stat.set_cmd = readl(aml_chan->mbox_fsts_addr);
	dev_dbg(dev, "%s: received cmd 0x%x\n", __func__, mbox_stat.set_cmd);
	if (mbox_stat.set_cmd) {
		aml_data.cmd = mbox_cmd->cmd;
		if (mbox_cmd->size >= MBOX_ASYNC_HEAD_SIZE) {
			aml_data.cmd = mbox_cmd->cmd;
			if (mbox_cmd->size > (MBOX_DATA_SIZE + MBOX_ASYNC_HEAD_SIZE))
				mbox_cmd->size = MBOX_DATA_SIZE + MBOX_ASYNC_HEAD_SIZE;
			mbox_fifo_read(&data_async, aml_chan->mbox_rd_addr, mbox_cmd->size);
			async_header = &data_async.async_header;

			if (!async_header->comp_info.complete)
				goto mbox_irq_handler_done;

			if (async_header->comp_info.complete == MBOX_NO_FEEDBACK)
				goto mbox_irq_handler_done;

			aml_data.rxsize = mbox_cmd->size - MBOX_ASYNC_HEAD_SIZE;
			aml_data.rxbuf = data_async.data;
			trans_mode_pattern =
				async_header->comp_info.drvid_info.trans_mode_pattern;
			/* It is DSP to ARM transmission */
			if (trans_mode_pattern == MBOX_TRANS_DSP2ARM) {
				drv_id = async_header->comp_info.drvid_info.drv_id;
				mbox_add_rx_queue(dev, drv_id, &aml_data);
			/* It is ARM to DSP TSYNC transmission */
			} else {
				complete = async_header->comp_info.complete;
				aml_data.rev_complete = complete;
				mbox_wakeup_wait_task(&aml_data);
			}
		}
	}
mbox_irq_handler_done:
	writel(~0, aml_chan->mbox_fclr_addr);
	return IRQ_HANDLED;
}

static irqreturn_t mbox_ack_irq_handler(int irq, void *p)
{
	struct aml_mbox_chan *aml_chan = p;
	struct mbox_chan *mbox_chan = container_of(aml_chan->tx_complete,
						   struct mbox_chan, tx_complete);
	struct aml_mbox_data *aml_data = mbox_chan->active_req;

	if (aml_data && aml_data->sync == MBOX_SYNC) {
		if (aml_data->rxbuf && aml_data->rxsize) {
			if (aml_data->rxsize > MBOX_DATA_SIZE) {
				dev_err(mbox_chan->cl->dev, "%s: rxsize %x > %x\n",
					__func__, aml_data->txsize, MBOX_DATA_SIZE);
				aml_data->rxsize = MBOX_DATA_SIZE;
			}
			mbox_fifo_read(aml_data->rxbuf,
				       aml_chan->mbox_rd_addr + MBOX_SYNC_HEAD_SIZE,
				       aml_data->rxsize);
		}
	}
	mbox_chan_txdone(mbox_chan, 0);
	return IRQ_HANDLED;
}

static int mbox_pl_parse_dt(struct platform_device *pdev, struct aml_chan_priv *priv)
{
	struct aml_mbox_chan *aml_chan;
	struct device *dev = &pdev->dev;
	const char *dir;
	u32 mbox_nums = 0;
	u32 rx_queue_len = 0;
	int idx = 0;
	int err = 0;

	err = of_property_read_u32(dev->of_node,
				   "mbox-nums", &mbox_nums);
	if (err) {
		dev_dbg(dev, "failed to get mbox num %d\n", err);
		return -ENXIO;
	}
	priv->mbox_nums = mbox_nums;
	aml_chan = devm_kzalloc(dev, sizeof(*aml_chan) * mbox_nums, GFP_KERNEL);
	if (IS_ERR(aml_chan))
		return -ENOMEM;
	for (idx = 0; idx < mbox_nums; idx++) {
		err = of_property_read_string_index(dev->of_node, "mbox-tx", idx, &dir);
		if (!err && !strncmp(dir, "tx", 2))
			aml_chan[idx].tx_chan = true;
		else
			aml_chan[idx].tx_chan = false;
		err = of_property_read_u32_index(dev->of_node, "mboxids",
						 idx, &aml_chan[idx].mboxid);
		if (err) {
			dev_dbg(dev, "mboxids define error\n");
			return err;
		}
		aml_chan[idx].mbox_irq = platform_get_irq(pdev, idx);
		aml_chan[idx].mbox_wr_addr = devm_platform_ioremap_resource(pdev, idx * 4 + 0);
		if (IS_ERR_OR_NULL(aml_chan[idx].mbox_wr_addr))
			return PTR_ERR(aml_chan[idx].mbox_wr_addr);
		aml_chan[idx].mbox_rd_addr = aml_chan[idx].mbox_wr_addr;

		aml_chan[idx].mbox_fset_addr = devm_platform_ioremap_resource(pdev, idx * 4 + 1);
		if (IS_ERR(aml_chan[idx].mbox_fset_addr))
			return PTR_ERR(aml_chan[idx].mbox_fset_addr);

		aml_chan[idx].mbox_fclr_addr = devm_platform_ioremap_resource(pdev, idx * 4 + 2);
		if (IS_ERR(aml_chan[idx].mbox_fclr_addr))
			return PTR_ERR(aml_chan[idx].mbox_fclr_addr);

		aml_chan[idx].mbox_fsts_addr = devm_platform_ioremap_resource(pdev, idx * 4 + 3);
		if (IS_ERR(aml_chan[idx].mbox_fsts_addr))
			return PTR_ERR(aml_chan[idx].mbox_fsts_addr);

		mutex_init(&aml_chan[idx].mutex);
		aml_chan[idx].tx_complete = NULL;
	}
	priv->aml_chan = aml_chan;

	err = of_property_read_u32(dev->of_node,
				   "mbox-rx-queue-length", &rx_queue_len);
	if (err) {
		dev_dbg(dev, "get rx queue length fail %d, set to default length\n",
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

static int mbox_pl_probe(struct platform_device *pdev)
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
	int err = 0;
	int idx0 = 0;
	int idx1 = 0;

	dev_dbg(dev, "mbox pl probe\n");

	err = mbox_pl_parse_dt(pdev, &aml_priv);
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
				if (!aml_chan[idx1].tx_chan)
					aml_chan[idx1].tx_complete = &mbox_chans[idx0].tx_complete;
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
	mbox_cons->ops = &mbox_pl_ops;
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
	mbox_rx_msg->thread =
		kthread_run(mbox_rx_process, dev, "mbox_rx_thread");
	if (IS_ERR_OR_NULL(mbox_rx_msg->thread)) {
		dev_dbg(dev, "Failed to create rx thread\n");
		return PTR_ERR(mbox_rx_msg->thread);
	}
	mbox_priv_data->rx_msg = mbox_rx_msg;

	platform_set_drvdata(pdev, mbox_priv_data);
	err = devm_mbox_controller_register(dev, mbox_cons);
	if (err) {
		dev_err(dev, "failed to register mailbox controller\n");
		return err;
	}

	for (idx0 = 0; idx0 < mbox_nums; idx0++) {
		aml_chan[idx0].mbox = mbox_cons;
		if (aml_chan[idx0].tx_chan) {
			err = request_threaded_irq(aml_chan[idx0].mbox_irq,
						   mbox_ack_irq_handler,
						   NULL, IRQF_ONESHOT | IRQF_NO_SUSPEND,
						   DRIVER_NAME, &aml_chan[idx0]);
			if (err) {
				dev_dbg(dev, "request irq error %x\n",
					aml_chan[idx0].mbox_irq);
				return err;
			}
		} else {
			err = request_threaded_irq(aml_chan[idx0].mbox_irq, mbox_irq_handler,
						   NULL, IRQF_ONESHOT | IRQF_NO_SUSPEND,
						   DRIVER_NAME, &aml_chan[idx0]);
			if (err) {
				dev_dbg(dev, "request irq error %x\n",
					aml_chan[idx0].mbox_irq);
				return err;
			}
		}
	}
	dev_dbg(dev, "pl mbox init done\n");
	return 0;
}

static void mbox_pl_remove(struct platform_device *pdev)
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

const struct mbox_domain c2_mbox_domains[] = {
	MBOX_DOMAIN(C2_REE2SE0, C2_MBOX_REE2SE, 0),
	MBOX_DOMAIN(C2_REE2SE1, C2_MBOX_REE2SE, 0),
	MBOX_DOMAIN(C2_REE2SE2, C2_MBOX_REE2SE, 0),
	MBOX_DOMAIN(C2_REE2SE3, C2_MBOX_REE2SE, 0),
	MBOX_DOMAIN(C2_REE2SE4, C2_MBOX_REE2SE, 0),
	MBOX_DOMAIN(C2_SE2REE0, C2_MBOX_SE2REE, 0),
	MBOX_DOMAIN(C2_SE2REE1, C2_MBOX_SE2REE, 0),
	MBOX_DOMAIN(C2_SE2REE2, C2_MBOX_SE2REE, 0),
	MBOX_DOMAIN(C2_SE2REE3, C2_MBOX_SE2REE, 0),
	MBOX_DOMAIN(C2_SE2REE4, C2_MBOX_SE2REE, 0),
	MBOX_DOMAIN(C2_DSPA2REE0, C2_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(C2_DSPA2REE1, C2_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(C2_DSPA2REE2, C2_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(C2_DSPA2REE3, C2_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(C2_DSPA2REE4, C2_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(C2_REE2DSPA0, C2_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(C2_REE2DSPA1, C2_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(C2_REE2DSPA2, C2_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(C2_REE2DSPA3, C2_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(C2_REE2DSPA4, C2_MBOX_REE2DSPA, 0),
};

static struct mbox_domain_data c2_mbox_domains_data __initdata = {
	.mbox_domains = c2_mbox_domains,
	.domain_counts = ARRAY_SIZE(c2_mbox_domains),
};

const struct mbox_domain a1_mbox_domains[] = {
	MBOX_DOMAIN(A1_DSPA2REE0, A1_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(A1_DSPA2REE1, A1_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(A1_DSPA2REE2, A1_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(A1_DSPA2REE3, A1_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(A1_DSPA2REE4, A1_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(A1_REE2DSPA0, A1_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(A1_REE2DSPA1, A1_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(A1_REE2DSPA2, A1_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(A1_REE2DSPA3, A1_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(A1_REE2DSPA4, A1_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(A1_DSPB2REE0, A1_MBOX_DSPB2REE, 0),
	MBOX_DOMAIN(A1_DSPB2REE1, A1_MBOX_DSPB2REE, 0),
	MBOX_DOMAIN(A1_DSPB2REE2, A1_MBOX_DSPB2REE, 0),
	MBOX_DOMAIN(A1_DSPB2REE3, A1_MBOX_DSPB2REE, 0),
	MBOX_DOMAIN(A1_DSPB2REE4, A1_MBOX_DSPB2REE, 0),
	MBOX_DOMAIN(A1_REE2DSPB0, A1_MBOX_REE2DSPB, 0),
	MBOX_DOMAIN(A1_REE2DSPB1, A1_MBOX_REE2DSPB, 0),
	MBOX_DOMAIN(A1_REE2DSPB2, A1_MBOX_REE2DSPB, 0),
	MBOX_DOMAIN(A1_REE2DSPB3, A1_MBOX_REE2DSPB, 0),
	MBOX_DOMAIN(A1_REE2DSPB4, A1_MBOX_REE2DSPB, 0),
};

static struct mbox_domain_data a1_mbox_domains_data __initdata = {
	.mbox_domains = a1_mbox_domains,
	.domain_counts = ARRAY_SIZE(a1_mbox_domains),
};

const struct mbox_domain c1_mbox_domains[] = {
	MBOX_DOMAIN(C1_DSPA2REE0, C1_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(C1_DSPA2REE1, C1_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(C1_DSPA2REE2, C1_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(C1_DSPA2REE3, C1_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(C1_DSPA2REE4, C1_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(C1_REE2DSPA0, C1_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(C1_REE2DSPA1, C1_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(C1_REE2DSPA2, C1_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(C1_REE2DSPA3, C1_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(C1_REE2DSPA4, C1_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(C1_DSPB2REE0, C1_MBOX_DSPB2REE, 0),
	MBOX_DOMAIN(C1_DSPB2REE1, C1_MBOX_DSPB2REE, 0),
	MBOX_DOMAIN(C1_DSPB2REE2, C1_MBOX_DSPB2REE, 0),
	MBOX_DOMAIN(C1_DSPB2REE3, C1_MBOX_DSPB2REE, 0),
	MBOX_DOMAIN(C1_DSPB2REE4, C1_MBOX_DSPB2REE, 0),
	MBOX_DOMAIN(C1_REE2DSPB0, C1_MBOX_REE2DSPB, 0),
	MBOX_DOMAIN(C1_REE2DSPB1, C1_MBOX_REE2DSPB, 0),
	MBOX_DOMAIN(C1_REE2DSPB2, C1_MBOX_REE2DSPB, 0),
	MBOX_DOMAIN(C1_REE2DSPB3, C1_MBOX_REE2DSPB, 0),
	MBOX_DOMAIN(C1_REE2DSPB4, C1_MBOX_REE2DSPB, 0),
};

static struct mbox_domain_data c1_mbox_domains_data __initdata = {
	.mbox_domains = c1_mbox_domains,
	.domain_counts = ARRAY_SIZE(c1_mbox_domains),
};

const struct mbox_domain tm2_mbox_domains_pl1[] = {
	MBOX_DOMAIN(TM2_DSPA2REE0, TM2_MBOX_DSPA2REE, 0),
	MBOX_DOMAIN(TM2_REE2DSPA0, TM2_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(TM2_REE2DSPA1, TM2_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(TM2_REE2DSPA2, TM2_MBOX_REE2DSPA, 0),
	MBOX_DOMAIN(TM2_DSPB2REE0, TM2_MBOX_DSPB2REE, 0),
	MBOX_DOMAIN(TM2_REE2DSPB0, TM2_MBOX_REE2DSPB, 0),
	MBOX_DOMAIN(TM2_REE2DSPB1, TM2_MBOX_REE2DSPB, 0),
	MBOX_DOMAIN(TM2_REE2DSPB2, TM2_MBOX_REE2DSPB, 0),
};

static struct mbox_domain_data tm2_mbox_domains_data __initdata = {
	.mbox_domains = tm2_mbox_domains_pl1,
	.domain_counts = ARRAY_SIZE(tm2_mbox_domains_pl1),
};

const struct mbox_domain g12a_mbox_domains_pl1[] = {
	MBOX_DOMAIN(G12A_REE2MF0, G12A_MBOX_REE2MF, 0),
	MBOX_DOMAIN(G12A_REE2MF1, G12A_MBOX_REE2MF, 0),
	MBOX_DOMAIN(G12A_REE2MF2, G12A_MBOX_REE2MF, 0),
	MBOX_DOMAIN(G12A_REE2MF3, G12A_MBOX_REE2MF, 0),
	MBOX_DOMAIN(G12A_REE2MF4, G12A_MBOX_REE2MF, 0),
};

static struct mbox_domain_data g12a_mbox_domains_data __initdata = {
	.mbox_domains = g12a_mbox_domains_pl1,
	.domain_counts = ARRAY_SIZE(g12a_mbox_domains_pl1),
};

static const struct of_device_id mbox_of_match[] = {
	{
		.compatible = "amlogic, c2-mbox-pl",
		.data = &c2_mbox_domains_data,
	},
	{
		.compatible = "amlogic, a1-mbox-pl",
		.data = &a1_mbox_domains_data,
	},
	{
		.compatible = "amlogic, c1-mbox-pl",
		.data = &c1_mbox_domains_data,
	},
	{
		.compatible = "amlogic, tm2-mbox-pl",
		.data = &tm2_mbox_domains_data,
	},
	{
		.compatible = "amlogic, g12a-mbox-pl",
		.data = &g12a_mbox_domains_data,
	},
	{}
};

static struct platform_driver mbox_pl_driver = {
	.probe  = mbox_pl_probe,
	.remove = mbox_pl_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name  = DRIVER_NAME,
		.of_match_table = mbox_of_match,
	},
};

int __init mbox_pl_v1_init(void)
{
	return platform_driver_register(&mbox_pl_driver);
}

void __exit mbox_pl_v1_exit(void)
{
	platform_driver_unregister(&mbox_pl_driver);
}
