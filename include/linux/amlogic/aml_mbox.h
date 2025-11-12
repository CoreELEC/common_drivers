/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_MBOX_H_
#define _AML_MBOX_H_
#include <linux/cdev.h>
#include <linux/completion.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox_controller.h>
#include "aml_mbox_cmd.h"
#include "aml_mbox_health.h"

#define MBOX_DATA_SIZE      0x60
#define MBOX_NO_FEEDBACK    0xA5A5A5A5

enum aml_mbox_sync {
	MBOX_ASYNC = 0,    /* Only send data, not get data from remote */
	MBOX_SYNC  = 1,    /* Send data, also can get data from remote if needed */
	MBOX_TSYNC = 2,    /* Send data, wait for data feedback, the wake up wait task */
};

/* mbox client id, only used for mbox_pl_v0 */
enum mbox_client_id {
	MBOX_CL_NONE,
	MBOX_CL_CLOCKS,
	MBOX_CL_DVFS,
	MBOX_CL_POWER,
	MBOX_CL_THERMAL,
	MBOX_CL_REMOTE,
	MBOX_CL_LED_TIMER,
	MBOX_CL_SET_CEC_DATA,
	MBOX_CL_MAX,
};

#define MBOX_CMD_ID_SHIFT       0
#define MBOX_CMD_ID_MASK        0xff
#define MBOX_CLIENT_ID_SHIFT    8
#define MBOX_CLIENT_ID_MASK     0xff
#define MBOX_PACK_CMD(cmd, client_id)					\
	((((cmd) & MBOX_CMD_ID_MASK) << MBOX_CMD_ID_SHIFT) | \
	 (((client_id) & MBOX_CLIENT_ID_MASK) << MBOX_CLIENT_ID_SHIFT))

/**
 * struct aml_mbox_data - mbox physical channel info
 * @cmd:               Use to remote mailbox driver recognize callback function
 * @txbuf:             Mbox tansfer data buffer
 * @rxbuf:             Mbox receive data buffer
 * @txsize:            Mbox transfer data size
 * @rxsize:            Mbox receive data size
 * @sync:              Transfer data to sync or not
 * @rev_complete:      Feedback complete address
 * @complete:          Use to process block, until resource release
 */
struct aml_mbox_data {
	u32 cmd;
	void *txbuf;
	void *rxbuf;
	u32 txsize;
	u32 rxsize;
	u8 sync;
	u64 rev_complete;
	struct completion complete;
};

/**
 * struct aml_mbox_chan - mbox physical channel info
 * @mboxid:               Mbox physical channel id
 * @irq_nums:             The total mbox irq nums
 * @mbox_irqctlr_addr:    Mbox irq controller address
 * @mbox_irqclr_addr:     Mbox irq clear address
 * @mbox_irq:             Mbox channel irq number
 * @tx_chan:              If transfer channel
 * @data:                 For payload mbox driver oldest version
 * @mbox_wr_addr:         Mbox data write address
 * @mbox_rd_addr:         Mbox data read address
 * @mbox_fset_addr:       Mbox set address
 * @mbox_fclr_addr:       Mbox clear address
 * @mbox_fsts_addr:       Mbox status address
 * @mutex:                Mbox channel mutex lock
 * @mbox:                 Mbox controller
 * @tx_complete:          Use to recognize the which channel send data
 * @aocpu_hang_detect:    If it support aocpu hang status detect
 * @dspa_hang_detect:     If it support dspa hang status detect
 * @dspb_hang_detect:     If it support dspb hang status detect
 * @dest:                 The processor which mailbox transmission send to
 */
struct aml_mbox_chan {
	/* mbox fifo */
	u32 mboxid;
	u32 irq_nums;
	void __iomem *mbox_irqctlr_addr;
	void __iomem *mbox_irqclr_addr;
	void __iomem *mbox_irqsts_addr;
	bool aocpu_hang_detect;

	/* mbox payload */
	u32 mbox_irq;
	bool tx_chan;
	void *data;

	/* mbox common struct */
	void __iomem *mbox_wr_addr;
	void __iomem *mbox_rd_addr;
	void __iomem *mbox_fset_addr;
	void __iomem *mbox_fclr_addr;
	void __iomem *mbox_fsts_addr;
	struct mutex mutex; /* for aml mbox chan mutex */
	struct mbox_controller *mbox;
	void *tx_complete;
	bool dspa_hang_detect;
	bool dspb_hang_detect;
	u32 dest;
};

/**
 * struct aml_mbox_rx_data - mbox rx data info
 * @cmd:        Mbox command which is sent from remote processor
 * @size:       Mbox message size sent from remote processor
 * @buf:        Mbox RX buffer which saved the data from remote processor
 */
struct aml_mbox_rx_data {
	u32 cmd;
	u32 size;
	char buf[MBOX_DATA_SIZE];
};

static inline void mbox_receive_callback(struct mbox_client *cl, void *mssg)
{
	dev_err(cl->dev, "Driver need add callback function\n");
}

/**
 * aml_mbox_request_channel_byidx - Request mbox channel by index
 * @dev: client device node
 * @name: mbox chan index
 * Return: The mbox channel if success
 */
static inline struct mbox_chan *aml_mbox_request_channel_byidx(struct device *dev, int idx)
{
	struct mbox_client *mbox_client;
	struct mbox_chan *mbox_chan;

	mbox_client = devm_kzalloc(dev, sizeof(*mbox_client), GFP_KERNEL);
	if (IS_ERR_OR_NULL(mbox_client))
		return ERR_PTR(-ENOMEM);
	mbox_client->dev = dev;
	mbox_client->tx_block = true;
	mbox_client->tx_tout = 10000;
	mbox_client->rx_callback = mbox_receive_callback;
	mbox_chan = mbox_request_channel(mbox_client, idx);
	if (IS_ERR_OR_NULL(mbox_chan)) {
		devm_kfree(dev, mbox_client);
		dev_dbg(dev, "Not find mbox channel byidx\n");
	}
	return mbox_chan;
}

/**
 * aml_mbox_request_channel_byname - Request mbox by name
 * @dev: client device node
 * @name: mbox chan name
 * Return: The mbox channel if success
 */
static inline struct mbox_chan *aml_mbox_request_channel_byname(struct device *dev, char *name)
{
	struct mbox_client *mbox_client;
	struct mbox_chan *mbox_chan;

	mbox_client = devm_kzalloc(dev, sizeof(*mbox_client), GFP_KERNEL);
	if (IS_ERR_OR_NULL(mbox_client))
		return ERR_PTR(-ENOMEM);
	mbox_client->dev = dev;
	mbox_client->tx_block = true;
	mbox_client->tx_tout = 10000;
	mbox_client->rx_callback = mbox_receive_callback;
	mbox_chan = mbox_request_channel_byname(mbox_client, name);
	if (IS_ERR_OR_NULL(mbox_chan)) {
		devm_kfree(dev, mbox_client);
		dev_dbg(dev, "Not find mbox channel byname\n");
	}
	return mbox_chan;
}

/**
 * aml_mbox_transfer_data - A way for transfer mbox data
 * @mbox_chan:            Mbox channel, this requested by mbox consumer driver
 * @cmd:                  Mbox cmd, use remote driver recognize callback function
 * @txbuf:                Mbox transfer data buffer
 * @txsize:               Mbox transfer data size
 * @rxbuf:                Mbox receive data size
 * @rxsize:               Mbox receive data size
 * @sync                  Mbox Sync info
 * return - 0:            Mbox transfer data success
 *        - less than 0:  Mbox transfer data fail
 */
static inline int aml_mbox_transfer_data(struct mbox_chan *mbox_chan, int cmd,
					 void *txbuf, u32 txsize, void *rxbuf, u32 rxsize,
					 enum aml_mbox_sync sync)
{
	struct aml_mbox_data aml_data;
	struct aml_mbox_chan *aml_chan;
	struct device *dev;
	int ret;
	u32 dest;
	bool ip_hang;

	if (IS_ERR_OR_NULL(mbox_chan)) {
		pr_err("mbox chan is NULL\n");
		return -EINVAL;
	}

	aml_chan = mbox_chan->con_priv;
	dest = aml_chan->dest;
	dev = mbox_chan->cl->dev;
	init_completion(&aml_data.complete);
	aml_data.sync = sync;
	aml_data.cmd = cmd;
	aml_data.txbuf = txbuf;
	aml_data.txsize = txsize;
	aml_data.rxbuf = rxbuf;
	aml_data.rxsize = rxsize;

	ip_hang = mbox_check_ip_hang(mbox_chan);
	if (ip_hang) {
		dev_err(dev, "error: dest processor %d is not boot up or hang\n",
				dest);
		return -ENODEV;
	}

	mutex_lock(&aml_chan->mutex);
	ret = mbox_send_message(mbox_chan, &aml_data);
	mutex_unlock(&aml_chan->mutex);
	if (ret < 0) {
		dev_err(dev, "Error: fail to send mbox data %d\n", ret);
		dev_warn(dev, "Warning: must not send new msg before TX ACK done!\n");
	}

	if (sync == MBOX_TSYNC)
		ret = wait_for_completion_killable(&aml_data.complete);
	return ret;
}

#endif
