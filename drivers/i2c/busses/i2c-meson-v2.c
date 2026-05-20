// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/pinctrl/consumer.h>
#include <linux/arm-smccc.h>
// #include <linux/amlogic/secure_pwm_i2c.h>
#include <dt-bindings/i2c/i2c-meson.h>
#include <linux/delay.h>

// #define I2C_DEBUG
// #define I2C_PXP
#define I2C_PM_RUNTIME

#define FUTURE_USE		0
/* Meson I2C register map */
#define REG_CFG_RDY		0x00
#define REG_CFG_I2C		0x04
#define REG_CFG_START		0x08
#define REG_CFG_BUS		0x0c
#define REG_TX_RD_ADDR		0x10
#define REG_TX_WR_ADDR		0x14
#define REG_RX_RD_ADDR		0x18
#define REG_RX_WR_ADDR		0x1c
#define REG_CGF_TX		0x20
#define REG_CGF_RX		0x24
#define REG_CGF_IRQ_STATE	0x30
#define REG_CGF_IRQ_ENABLE	0x34
#define REG_SHAKE_BLK_CNT	0x38 //could check numbers of bytes send

/* CFG RDY fields */
#define RDY_TEE_ONLY		BIT(1)
#define RDY_IF			BIT(0)

/* CFG I2C fields */
#define I2C_RX_THR		GENMASK(23, 16)
#define I2C_TX_THR		GENMASK(15, 8)
#define I2C_DMA			BIT(4)
#define I2C_HW_NEG		BIT(3)
#define I2C_HW_POS		BIT(2)
#define I2C_JOIN_RX		BIT(1)
#define I2C_JOIN_TX		BIT(0)
#define I2C_TX_THR_DIV		2

/* CFG BUS fields */
#define BUS_NO_STOP		BIT(18)//after soc c5 support this bit. others invalid.
#define BUS_SPEED_MODE		BIT(17)
#define BUS_SLAVE_MODE		BIT(16)
#define BUS_FILTER_MASK		GENMASK(15, 12)
/*SCL = clk/b_ratio	if b_ratio<=8, SCL = clk/8 */
#define BUS_RATIO_MASK		GENMASK(11, 0)

/* CFG START fields */
#define START_STRAT		BIT(31)
#define START_LEN		GENMASK(23, 12)
#define START_LEN_SHIFT		12
#define START_10_BITS		BIT(11) /*only 7bit now*/
#define START_SLAVE_ADDR	GENMASK(10, 1)
#define START_READ		BIT(0)

/* CFG TX_RX fields */
#define TX_RX_EMPTY		BIT(9)
#define TX_RX_FULL		BIT(8)
#define TX_RX_DATA		GENMASK(7, 0)

/* SHAKE_BLK_CNT fields */
#define BLK_CNT_MASK		GENMASK(16, 4)
#define BLK_CNT_SHIFTS		4

/*CGF IRQ fields */
#define RCH_ERROR		BIT(0)
#define WCH_ERROR		BIT(1)
#define NCK_ERROR		BIT(2)
#define RX_EMPTY		BIT(3)
#define TX_FULL			BIT(4)
#define RX_FIFO_READ		BIT(5)
#define TX_FIFO_WRITE		BIT(6)
#define PHY_DONE		BIT(7)

#define TRANS_ERROR		(RCH_ERROR | WCH_ERROR | NCK_ERROR)
#define TRANS_DONE		(PHY_DONE | TRANS_ERROR)

/* enable irq, critical trans done bit must be set.
 * tx/rx thresh bit used for continuously transfer,
 * set it manul when transfer.
 * TX_FULL RX_EMPTY not used.
 * NACK ERROR can not be use in gxlx4, it will cause down signal at scl after nack.
 */
#define ENABLE_IRQ_BIT	(NCK_ERROR | RCH_ERROR | WCH_ERROR | PHY_DONE)
//| RX_FIFO_READ | TX_FIFO_WRITE)

/*A9 and new IRQ fields*/
#define A9_NCK_ERROR		BIT(0)
#define A9_RX_EMPTY		BIT(1)
#define A9_RX_FULL		BIT(2)
#define A9_TX_EMPTY		BIT(3)
#define A9_TX_FULL		BIT(4)
//#define A9_RX_FIFO_READ	BIT(5) same as the old
//#define A9_TX_FIFO_WRITE	BIT(6) same as the old
//#define A9_PHY_DONE		BIT(7) same as the old
#define A9_TASK_DONE		BIT(8)
#define A9_ALL_DONE		BIT(9)

#define A9_TRANS_DONE		(PHY_DONE | A9_NCK_ERROR)
#define A9_TRANS_ERROR		(A9_NCK_ERROR)
#define A9_ENABLE_IRQ_BIT	(A9_NCK_ERROR | PHY_DONE)

//if use thresh mode, should be set at start.
#define THRESH_MODE		(RX_FIFO_READ | TX_FIFO_WRITE)
#define REG_FIFO_BASE		0x80

/* keep the same with I2C_TIMEOUT_MS */
#define MESON_I2C_PM_TIMEOUT	500
#define I2C_TIMEOUT_MS		500
#define MESON_I2C_PM_TIMEOUT	500

enum {
	STATE_IDLE,
	STATE_READ,
	STATE_WRITE,
	STATE_WAIT,
};

struct meson_i2c_data {
	bool tee;
	u32 tee_id;
	u32 enable_irq_bits;
	u32 xfer_done_bits;
	u32 xfer_err_bits;
};

/**
 * struct meson_i2c - Meson I2C device private data
 *
 * @adap:	I2C adapter instance
 * @dev:	Pointer to device structure
 * @regs:	Base address of the device memory mapped registers
 * @clk:	Pointer to clock structure
 * @msg:	Pointer to the current I2C message
 * @state:	Current state in the driver state machine
 * @count:	Number of bytes to be sent/received in current transfer
 * @last:	The last msg flag
 * @pos:	Current position in the send/receive buffer
 * @error:	Flag set when an error is received
 * @lock:	To avoid race conditions between irq handler and xfer code
 * @done:	Completion used to wait for transfer termination
 * @frequency:	Operating frequency of I2C bus clock
 * @data:	Pointer to the controller's platform data
  *@mode:	I2C's transfer mode irq or polling
 */
struct meson_i2c {
	struct i2c_adapter	adap;
	struct device		*dev;
	void __iomem		*regs;
	struct clk		*clk;

	struct i2c_msg		*msg;
	int			state;
	int			count;
	int			pos;
	bool		last;
	int			error;
	int			fifo_depth;

	spinlock_t		lock;		//protection for i2c controller critical section
	struct completion	done;
#ifdef CONFIG_AMLOGIC_MODIFY
	unsigned long		clk_rate;
	unsigned int		frequency;
	int retain_fastmode;
	int irq;
	bool is_runtime_sleep;
#endif
	struct meson_i2c_data		*data;
	int mode;

};

static void meson_i2c_writel(struct meson_i2c *i2c, u32 data, int reg)
{
#if FUTURE_USE
	struct arm_smccc_res res;

	if (i2c->data->tee)
		arm_smccc_smc(SECURE_PWM_I2C, SECID_I2C, i2c->data->tee_id,
			reg, data, 0, 0, 0, &res);
	else
#endif
		writel(data, i2c->regs + reg);
}

static inline void meson_i2c_dump_reg(struct meson_i2c *i2c, const char *func, int line)
{
#ifdef I2C_DEBUG
	u32 data;
	int i;

	trace_printk("%s %s, line:%d\n", dev_name(i2c->dev), func, line);
	for (i = 0 ; i < 8; i++) {
		data = readl(i2c->regs + i * 4);
		trace_printk("%s i2c reg%x : 0x%x\n", dev_name(i2c->dev), i, data);
	}
	for (i = 0xb ; i < 0xf; i++) {
		data = readl(i2c->regs + i * 4);
		trace_printk("%s i2c reg%x : 0x%x\n", dev_name(i2c->dev), i, data);
	}
#endif
}

static void meson_i2c_set_mask(struct meson_i2c *i2c, int reg, u32 mask,
			       u32 val)
{
	u32 data;

	data = readl(i2c->regs + reg);
	data &= ~mask;
	data |= val & mask;
	meson_i2c_writel(i2c, data, reg);
}

/*
 * SCL = clk/b_ratio. if b_ratio<=8, SCL = clk/8
 */
static void meson_i2c_set_clk_div(struct meson_i2c *i2c)
{
	unsigned long clk_rate = i2c->clk_rate;
	unsigned int div;

	div = DIV_ROUND_UP(clk_rate, i2c->frequency);

	/* clock divider has 12 bits */
	if (div >= (1 << 12))
		dev_err(i2c->dev, "requested bus frequency too low\n");
	else if (div <= 8)
		dev_err(i2c->dev, "requested bus frequency too high\n");
#ifdef I2C_PXP
	div = 0x40;
#endif
	/* CFG_BUS reg:11 - 0 bits
	 * BUS_SPEED_MODE for HW timming
	 */
	meson_i2c_set_mask(i2c, REG_CFG_BUS,
		BUS_RATIO_MASK | BUS_SPEED_MODE, div | BUS_SPEED_MODE);
#ifdef I2C_DEBUG
	trace_printk("%s %s: clk %lu, freq %u, div %u\n",
		dev_name(i2c->dev), __func__,
			clk_rate, i2c->frequency, div);
#endif
}

static void reset_fifo_pos(struct meson_i2c *i2c)
{
	meson_i2c_writel(i2c, 0x00, REG_TX_RD_ADDR);
	meson_i2c_writel(i2c, 0x00, REG_TX_WR_ADDR);

	/* Must reset rx_wr_addr before rx_rd_addr for continues read.
	 * Because reset rd will pull data to fifo, so we need reset wr early.
	 * And then, data will be push rightly to 1st fifo position.
	 */
	meson_i2c_writel(i2c, 0x00, REG_RX_WR_ADDR);
	meson_i2c_writel(i2c, 0x00, REG_RX_RD_ADDR);
}

static int controller_is_error(struct meson_i2c *i2c)
{
	int ret = 0;

	if (readl(i2c->regs + REG_CGF_IRQ_STATE) & i2c->data->xfer_err_bits) {
#ifdef I2C_DEBUG
		trace_printk("%s i2c controller error: 0x%x\n", dev_name(i2c->dev),
					readl(i2c->regs + REG_CGF_IRQ_STATE));
#endif
		ret = -ENXIO;
	}

	return ret;
}

static int controller_is_done(struct meson_i2c *i2c)
{
	return readl(i2c->regs + REG_CGF_IRQ_STATE) & i2c->data->xfer_done_bits;
}

static void clear_irq_status(struct meson_i2c *i2c, u32 irq_bits)
{
	meson_i2c_writel(i2c, irq_bits, REG_CGF_IRQ_STATE);
}

static int wait_controller_done(struct meson_i2c *i2c)
{
	u32 time_cnt = 0;
	int ret = 0;

	while (!controller_is_done(i2c)) {
		if (time_cnt > I2C_TIMEOUT_MS * 1000)
			return -ETIMEDOUT;
		udelay(1);
		time_cnt++;
	}
	ret = controller_is_error(i2c);

	return ret;
}

static int get_tx_count(struct meson_i2c *i2c)
{
	int rd_val;

	rd_val = (readl(i2c->regs + REG_SHAKE_BLK_CNT) & BLK_CNT_MASK) >> BLK_CNT_SHIFTS;

	return rd_val;
}

static int wait_tx_done(struct meson_i2c *i2c, u32 des_count)
{
	u32 time_cnt = 0;
	int ret = 0;

	while (get_tx_count(i2c) != des_count) {
		if (time_cnt > I2C_TIMEOUT_MS * 1000)
			return -ETIMEDOUT;
		udelay(1);
		time_cnt++;
	}

	return ret;
}

static void meson_i2c_init(struct meson_i2c *i2c)
{
	meson_i2c_set_mask(i2c, REG_CFG_BUS, BUS_FILTER_MASK, 1 << 12);
	clear_irq_status(i2c, 0xffff);//clear all irq
	meson_i2c_writel(i2c, i2c->data->enable_irq_bits, REG_CGF_IRQ_ENABLE);
	meson_i2c_set_mask(i2c, REG_CFG_BUS, BUS_FILTER_MASK | BUS_NO_STOP, 1 << 12 | BUS_NO_STOP);
	/* set rx thresh, when fifo has data and fifo full will generate irq for cpu read.
	 * set tx thresh, when fifo empty will generate irq for continue transfer.
	 * set fifo depth and should clear dma mode avoid tcon driver set DMA,
	 * because TCON driver will OP i2c DMA in there driver.
	 */
	i2c->fifo_depth = 16;
	meson_i2c_set_mask(i2c, REG_CFG_I2C, I2C_DMA | I2C_RX_THR | I2C_TX_THR,
				i2c->fifo_depth << 16 | (i2c->fifo_depth / I2C_TX_THR_DIV) << 8);
	reset_fifo_pos(i2c);
	meson_i2c_set_clk_div(i2c);
}

static void meson_i2c_add_stop(struct meson_i2c *i2c)
{
	meson_i2c_set_mask(i2c, REG_CFG_BUS, BUS_NO_STOP, 0);
}

static void meson_i2c_push_data_to_user(struct meson_i2c *i2c, u8 data)
{
	u8 *buf = i2c->msg->buf + i2c->pos;

	i2c->count = 1;
	*buf = data;
#ifdef I2C_DEBUG
	trace_printk("%s %s: data %08x\n", dev_name(i2c->dev), __func__, *buf);
#endif
}

static void meson_i2c_put_data(struct meson_i2c *i2c, char *buf, int len)
{
	if (!i2c->msg->len) {//this i2c module when trans 0 byte, must put at least 1.
		meson_i2c_writel(i2c, 0x00, REG_CGF_TX);
		return;
	}
	for (int i = 0; i < len; i++, buf++) {
		meson_i2c_writel(i2c, *buf, REG_CGF_TX);
#ifdef I2C_DEBUG
		trace_printk("%s %s: data %08x\n", dev_name(i2c->dev), __func__, *buf);
#endif
	}
}

static void meson_i2c_prepare_xfer(struct meson_i2c *i2c, bool full_fill)
{
	bool write = !(i2c->msg->flags & I2C_M_RD);

	if (write) {
		if (i2c->mode == I2C_POLLING_MODE || full_fill)
			i2c->count = min(i2c->msg->len - i2c->pos, i2c->fifo_depth);
		else
			i2c->count = min(i2c->msg->len - i2c->pos,
					i2c->fifo_depth / I2C_TX_THR_DIV);
		meson_i2c_put_data(i2c, i2c->msg->buf + i2c->pos, i2c->count);
	}
}

/* three case should be check in irq:
 * 1.trans done
 * 2.RX thresh(fifo full) ------> pull data
 * 3.TX thresh(fifo empty) ------>push data to fifo
 *
 */

static irqreturn_t meson_i2c_irq(int irqno, void *dev_id)
{
	struct meson_i2c *i2c = dev_id;
	int ret;
	u32 rd_len, i;
	u32 rd_val;

	spin_lock(&i2c->lock);
	ret = controller_is_error(i2c);
	if (ret) {
#ifdef I2C_DEBUG
		trace_printk("%s IRQ controller error\n", dev_name(i2c->dev));
#endif
		i2c->error = ret;
		goto complete_irq_out;
	}

	switch (i2c->state) {
	case STATE_READ:
		meson_i2c_dump_reg(i2c, __func__, __LINE__);
		rd_len = min(i2c->msg->len - i2c->pos, i2c->fifo_depth);
#ifdef I2C_DEBUG
		trace_printk("%s thresh for rx rd-len:%d\n",  dev_name(i2c->dev), rd_len);
#endif
		for (i = 0; i < rd_len; i++) {
			rd_val = readl(i2c->regs + REG_CGF_RX);
#ifdef I2C_DEBUG
			trace_printk("%s thresh for rx RX reg value:0x%x\n",
				dev_name(i2c->dev), rd_val);
#endif
			meson_i2c_push_data_to_user(i2c, rd_val & TX_RX_DATA);
			i2c->pos += i2c->count;
		}
		clear_irq_status(i2c, RX_FIFO_READ);//rx must clear thresh after read fifo
		break;
	case STATE_WRITE:
#ifdef I2C_DEBUG
		trace_printk("%s thresh for tx\n",  dev_name(i2c->dev));
#endif
		meson_i2c_dump_reg(i2c, __func__, __LINE__);
		meson_i2c_prepare_xfer(i2c, false);
		if ((i2c->pos + i2c->fifo_depth / I2C_TX_THR_DIV) >= i2c->msg->len)
			meson_i2c_set_mask(i2c, REG_CGF_IRQ_ENABLE, TX_FIFO_WRITE, 0);
		clear_irq_status(i2c, TX_FIFO_WRITE);//tx must clear thresh after write fifo
		i2c->pos += i2c->count;
		break;
	case STATE_IDLE:
		dev_warn(i2c->dev, "i2c irq triggered abnormally!!!\n");
		break;
	default:
		break;
	}
	//check controller done?
	if (!controller_is_done(i2c)) {
		goto irq_trans_out;
	} else {
#ifdef I2C_DEBUG
		trace_printk("%s i2c irq controller_is_done\n",
					dev_name(i2c->dev));
#endif
		if (i2c->state == STATE_READ && (i2c->msg->len - i2c->pos)) {//read not complete
#ifdef I2C_DEBUG
			trace_printk("%s i2c irq done but read not complete\n",
					dev_name(i2c->dev));
#endif
			goto irq_trans_out;
		}
	}

complete_irq_out:
	reset_fifo_pos(i2c);
	complete(&i2c->done);
	i2c->state = STATE_IDLE;
	meson_i2c_set_mask(i2c, REG_CGF_IRQ_ENABLE,
		RX_FIFO_READ | TX_FIFO_WRITE, 0);//disable TX/RX irq bits
	clear_irq_status(i2c, 0xffff);//clear all irq
irq_trans_out:
#ifdef I2C_DEBUG
	trace_printk("%s IRQ out\n", dev_name(i2c->dev));
#endif
	meson_i2c_dump_reg(i2c, __func__, __LINE__);
	spin_unlock(&i2c->lock);

	return IRQ_HANDLED;
}

static void meson_i2c_do_start(struct meson_i2c *i2c, struct i2c_msg *msg)
{
	unsigned int reg_val;
	unsigned int trans_len;

	trans_len = msg->len;
	if (i2c->mode == I2C_POLLING_MODE) {
		//if TX and not last xfer, need add no stop
		if (!(msg->flags & I2C_M_RD) && !i2c->last && msg->len)
			trans_len = msg->len + 1;
	}
	reg_val = (msg->addr << 1) |
				((msg->flags & I2C_M_RD) ? START_READ : 0) |
				((trans_len) << 12) | START_STRAT;
	meson_i2c_set_mask(i2c, REG_CFG_START,
		START_STRAT | START_SLAVE_ADDR | START_READ | START_LEN, reg_val);
	//NACK irq will be clear validly only after start signal
	clear_irq_status(i2c, 0xffff);//clear all irq
	if (trans_len > i2c->fifo_depth) {
		reg_val = (msg->flags & I2C_M_RD) ? RX_FIFO_READ : TX_FIFO_WRITE;
		meson_i2c_set_mask(i2c, REG_CGF_IRQ_ENABLE,
		RX_FIFO_READ | TX_FIFO_WRITE, reg_val);//enable thresh mode
	}
	meson_i2c_dump_reg(i2c, __func__, __LINE__);
}

static int meson_i2c_xfer_msg(struct meson_i2c *i2c, struct i2c_msg *msg, bool last)
{
	unsigned long time_left, flags;
	int ret = 0;

	i2c->msg = msg;
	i2c->pos = 0;
	i2c->count = 0;
	i2c->error = 0;

	i2c->state = (msg->flags & I2C_M_RD) ? STATE_READ : STATE_WRITE;
	meson_i2c_prepare_xfer(i2c, true);
	i2c->pos += i2c->count;//every prepare data pushed, should increase pos.
	reinit_completion(&i2c->done);

	if (last)
		meson_i2c_add_stop(i2c);
	/* Start the transfer */
	meson_i2c_do_start(i2c, msg);

	time_left = msecs_to_jiffies(I2C_TIMEOUT_MS);
	time_left = wait_for_completion_timeout(&i2c->done, time_left);

	/*
	 * Protect access to i2c struct and registers from interrupt
	 * handlers triggered by a transfer terminated after the
	 * timeout period
	 */
	spin_lock_irqsave(&i2c->lock, flags);

	if (!time_left) {
		/*to do. maybe reset i2c for stop*/
		i2c->state = STATE_IDLE;
		ret = -ETIMEDOUT;
	}

	if (i2c->error)
		ret = i2c->error;

	spin_unlock_irqrestore(&i2c->lock, flags);

	return ret;
}

static int meson_i2c_no_stop(struct meson_i2c *i2c)
{
	int ret = 0;
	int trans_cnt;
	u32 time_cnt = 0;

	//check data trans done
	while (true) {
		trans_cnt = (readl(i2c->regs + REG_SHAKE_BLK_CNT) & BLK_CNT_MASK) >> BLK_CNT_SHIFTS;
		if (trans_cnt >= i2c->msg->len)
			break;
		/*time out*/
		if (time_cnt > I2C_TIMEOUT_MS * 1000) {
			ret =  -ETIMEDOUT;
			goto no_stop_out;
		}
		udelay(1);
		time_cnt++;
	}
	udelay(20);//must delay controller idle
	pinctrl_pm_select_sleep_state(i2c->dev);//release SDA/SCL line
	meson_i2c_writel(i2c, i2c->msg->len << START_LEN_SHIFT, REG_CFG_START);//stop controller
	meson_i2c_writel(i2c, 0xaf, REG_CGF_TX);//write visual xfer
	//wait trans done
	ret = wait_controller_done(i2c);

no_stop_out:
	reset_fifo_pos(i2c);
	pinctrl_pm_select_default_state(i2c->dev);

	return ret;
}

static int meson_i2c_xfer_polling_msg(struct meson_i2c *i2c, struct i2c_msg *msg, bool last)
{
	int rd_val, time_cnt, ret = 0;

	i2c->msg = msg;
	i2c->pos = 0;
	i2c->count = 0;
	i2c->last = last;
	meson_i2c_do_start(i2c, msg);
	do {
		if (msg->flags & I2C_M_RD) {//for rx
			time_cnt = 0;
			do {
				rd_val = readl(i2c->regs + REG_CGF_RX);
				if (time_cnt > I2C_TIMEOUT_MS * 1000) {
					ret = -ETIMEDOUT;
					goto xfer_out;
				}
				udelay(1);
				time_cnt++;
				ret = controller_is_error(i2c);
				if (ret)
					goto xfer_out;
			} while (rd_val & TX_RX_EMPTY);//waiting data
			meson_i2c_push_data_to_user(i2c, rd_val & TX_RX_DATA);
		} else { //for tx
			meson_i2c_prepare_xfer(i2c, true);
			ret = wait_tx_done(i2c, i2c->pos + i2c->count);
			if (ret)
				goto xfer_out;
		}
		i2c->pos += i2c->count;
	} while (i2c->pos < msg->len);

	//if TX and not last xfer, need add no stop
	if (!(msg->flags & I2C_M_RD) && !i2c->last && msg->len) {
		ret = meson_i2c_no_stop(i2c);
		if (ret)
			goto xfer_out;
	} else {
		//wait trans done
		ret = wait_controller_done(i2c);
		if (ret)
			goto xfer_out;
	}

xfer_out:
	clear_irq_status(i2c, 0xffff);//clear all irq
	reset_fifo_pos(i2c);

	return ret;
}

static int meson_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs,
			  int num)
{
	struct meson_i2c *i2c = adap->algo_data;
	int i, ret = 0;

#ifdef I2C_PM_RUNTIME
	ret = pm_runtime_get_sync(i2c->dev);
	if (ret < 0)
		goto out;
#endif
	meson_i2c_init(i2c);//must init i2c in every xfer
	for (i = 0; i < num; i++) {
		if (i2c->mode == I2C_POLLING_MODE)
			ret = meson_i2c_xfer_polling_msg(i2c, msgs + i, i == num - 1);
		else
			ret = meson_i2c_xfer_msg(i2c, msgs + i, i == num - 1);
		if (ret)
			break;
	}

#ifdef I2C_PM_RUNTIME
out:
	pm_runtime_mark_last_busy(i2c->dev);
	pm_runtime_put_autosuspend(i2c->dev);
#endif
	return ret ?: i;
}

static u32 meson_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm meson_i2c_algorithm = {
	.master_xfer	= meson_i2c_xfer,
	.functionality	= meson_i2c_func,
};

static ssize_t speed_show(struct device *child,
			  struct device_attribute *attr, char *buf)
{
	struct meson_i2c *i2c =
		(struct meson_i2c *)dev_get_drvdata(child);
	return sprintf(buf, "%d\n", i2c->frequency);
}

static ssize_t speed_store(struct device *child,
			   struct device_attribute *attr,
			   const char *buf, size_t size)
{
	struct meson_i2c *i2c =
		(struct meson_i2c *)dev_get_drvdata(child);
	int ret, val;

	ret = kstrtouint(buf, 0, &val);
	if (ret)
		return ret;

	i2c->frequency = val;
	meson_i2c_set_clk_div(i2c);

	return size;
}

static DEVICE_ATTR_RW(speed);

static int meson_i2c_speed_debug(struct device *dev)
{
	return sysfs_create_file(&dev->kobj, &dev_attr_speed.attr);
}

static int meson_i2c_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct meson_i2c *i2c;
	struct i2c_timings timings;
	int irq, ret;

	i2c = devm_kzalloc(&pdev->dev, sizeof(struct meson_i2c), GFP_KERNEL);
	if (!i2c)
		return -ENOMEM;

	i2c_parse_fw_timings(&pdev->dev, &timings, true);
	i2c->frequency = timings.bus_freq_hz;
	i2c->data = (struct meson_i2c_data *)of_device_get_match_data(&pdev->dev);
	i2c->dev = &pdev->dev;
	platform_set_drvdata(pdev, i2c);

	spin_lock_init(&i2c->lock);
	init_completion(&i2c->done);

	i2c->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(i2c->clk)) {
		dev_err(&pdev->dev, "can't get device clock\n");
		return PTR_ERR(i2c->clk);
	}
	i2c->regs  = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(i2c->regs))
		return PTR_ERR(i2c->regs);
	if (i2c->data->tee) {
		/* get i2c tee_id property */
		ret = of_property_read_u32(pdev->dev.of_node, "tee_id",
				   &i2c->data->tee_id);
		if (ret) {
			dev_err(&pdev->dev, "not config tee_id\n");
			return ret;
		}
	}
	//init i2c, avoid it's old states come from uboot
	meson_i2c_init(i2c);
	if (of_property_read_u32(pdev->dev.of_node, "working_mode",
				 &i2c->mode)) {
		i2c->mode = I2C_IRQ_MODE;
		irq = platform_get_irq(pdev, 0);
		if (irq < 0) {
			dev_err(&pdev->dev, "can't find IRQ\n");
			return irq;
		}
		ret = devm_request_irq(&pdev->dev, irq, meson_i2c_irq, 0, NULL, i2c);
		if (ret < 0) {
			dev_err(&pdev->dev, "can't request IRQ\n");
			return ret;
		}
		i2c->irq = irq;
#ifdef I2C_PM_RUNTIME
		/* Disable the interrupt so that the system can enter low-power mode */
		disable_irq(i2c->irq);
#endif
	}

	strscpy(i2c->adap.name, "Meson I2C V2 adapter",
		sizeof(i2c->adap.name));
	i2c->adap.owner = THIS_MODULE;
	i2c->adap.algo = &meson_i2c_algorithm;
	i2c->adap.dev.parent = &pdev->dev;
	i2c->adap.dev.of_node = np;
	i2c->adap.algo_data = i2c;
#ifdef I2C_PM_RUNTIME
	pm_runtime_enable(i2c->dev);
	pm_runtime_set_autosuspend_delay(i2c->dev, MESON_I2C_PM_TIMEOUT);
	pm_runtime_use_autosuspend(i2c->dev);

	ret = pm_runtime_get_sync(i2c->dev);
	if (ret < 0) {
		pm_runtime_put_noidle(i2c->dev);
		pm_runtime_disable(i2c->dev);
		return ret;
	}
#endif
	/* speed sysfs */
	ret = meson_i2c_speed_debug(&pdev->dev);
	if (ret)
		dev_err(&pdev->dev, "Creat i2c speed debug sysfs failed\n");
	i2c->clk_rate = clk_get_rate(i2c->clk);
	if (i2c->clk_rate == 0) {
		dev_err(&pdev->dev, "failed to get clk rate\n");
		return -EINVAL;
	}
#ifdef I2C_PM_RUNTIME
	pm_runtime_mark_last_busy(i2c->dev);
	pm_runtime_put_autosuspend(i2c->dev);
#endif
	ret = i2c_add_adapter(&i2c->adap);
	if (ret < 0)
		return ret;
	return 0;
}

static void meson_i2c_remove(struct platform_device *pdev)
{
	struct meson_i2c *i2c = platform_get_drvdata(pdev);
#ifdef I2C_PM_RUNTIME
	int ret;

	ret = pm_runtime_get_sync(&pdev->dev);
	if (ret < 0)
		return;
#endif
	i2c_del_adapter(&i2c->adap);
#ifdef I2C_PM_RUNTIME
	pm_runtime_dont_use_autosuspend(&pdev->dev);
	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
#endif
}

static int meson_i2c_put(struct meson_i2c *i2c)
{
	if (i2c->mode == I2C_IRQ_MODE)
		disable_irq(i2c->irq);
	pinctrl_pm_select_sleep_state(i2c->dev);
	clk_disable_unprepare(i2c->clk);

	return 0;
}

static int meson_i2c_regain(struct meson_i2c *i2c)
{
	int ret;

	ret = clk_prepare_enable(i2c->clk);
	if (ret < 0) {
		dev_err(i2c->dev, "can not enable clock\n");
		return ret;
	}
	pinctrl_pm_select_default_state(i2c->dev);
	if (i2c->mode == I2C_IRQ_MODE)
		enable_irq(i2c->irq);


	return 0;
}

#ifdef CONFIG_HIBERNATION
static int meson_i2c_freeze(struct device *dev)
{
	int ret;
	struct meson_i2c *i2c = (struct meson_i2c *)dev_get_drvdata(dev);

	if (i2c->is_runtime_sleep)
		return 0;
	ret = meson_i2c_put(i2c);

	return ret;
}

static int meson_i2c_thaw(struct device *dev)
{
	return 0;
}

static int meson_i2c_restore(struct device *dev)
{
	int ret;
	struct meson_i2c *i2c = (struct meson_i2c *)dev_get_drvdata(dev);

	if (i2c->is_runtime_sleep)
		return 0;
	ret = meson_i2c_regain(i2c);

	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_MODIFY
static int __maybe_unused meson_i2c_runtime_suspend(struct device *dev)
{
	int ret;
	struct meson_i2c *i2c = (struct meson_i2c *)dev_get_drvdata(dev);

	ret = meson_i2c_put(i2c);
	i2c->is_runtime_sleep = true;

	return ret;
}

static int __maybe_unused meson_i2c_runtime_resume(struct device *dev)
{
	int ret;
	struct meson_i2c *i2c = (struct meson_i2c *)dev_get_drvdata(dev);

	ret = meson_i2c_regain(i2c);
	i2c->is_runtime_sleep = false;

	return ret;
}

static const struct dev_pm_ops meson_i2c_pm_ops = {
	SET_RUNTIME_PM_OPS(meson_i2c_runtime_suspend,
			   meson_i2c_runtime_resume, NULL)
#ifdef CONFIG_HIBERNATION
	.freeze		= meson_i2c_freeze,
	.thaw		= meson_i2c_thaw,
	.restore	= meson_i2c_restore,
#endif
};
#endif

static struct meson_i2c_data i2c_tee_data = {
	.tee = true,
	.enable_irq_bits = ENABLE_IRQ_BIT,
	.xfer_done_bits = TRANS_DONE,
	.xfer_err_bits = TRANS_ERROR,
};

static struct meson_i2c_data i2c_normal_data = {
	.tee = false,
	.enable_irq_bits = ENABLE_IRQ_BIT,
	.xfer_done_bits = TRANS_DONE,
	.xfer_err_bits = TRANS_ERROR,
};

static struct meson_i2c_data i2c_normal_a9 = {
	.tee = false,
	.enable_irq_bits = A9_ENABLE_IRQ_BIT,
	.xfer_done_bits = A9_TRANS_DONE,
	.xfer_err_bits = A9_TRANS_ERROR,
};

static const struct of_device_id meson_i2c_v2_match[] = {
	{	.compatible = "amlogic,meson-i2c-v2",
		.data = &i2c_normal_data
	},
	{
		.compatible = "amlogic,meson-tee-i2c-v2",
		.data = &i2c_tee_data
	},
	{	.compatible = "amlogic,meson-i2c-v2-1",
		.data = &i2c_normal_a9
	},
	{},
};

MODULE_DEVICE_TABLE(of, meson_i2c_v2_match);

static struct platform_driver meson_i2c_driver = {
	.probe   = meson_i2c_probe,
	.remove  = meson_i2c_remove,
	.driver  = {
		.name  = "meson-i2c-v2",
		.pm	= &meson_i2c_pm_ops,
		.of_match_table = meson_i2c_v2_match,
	},
};

int __init i2c_meson_v2_init(void)
{
	return platform_driver_register(&meson_i2c_driver);
}

void __exit i2c_meson_v2_exit(void)
{
	platform_driver_unregister(&meson_i2c_driver);
}

MODULE_DESCRIPTION("Amlogic Meson I2C V2 Bus driver");
MODULE_AUTHOR("Junyi Zhao <junyi.zhao@amlogic.com>");
MODULE_LICENSE("GPL v2");
