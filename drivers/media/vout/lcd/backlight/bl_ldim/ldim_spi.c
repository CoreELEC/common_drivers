// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/dma-mapping.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/aml_spi.h>
#include "ldim_drv.h"
#include "ldim_dev_drv.h"
#include "../../lcd_common.h"

#define SPI_SAFE_LINE_MARGIN 10

int ldim_spi_dma_cycle_align_byte(int size)
{
	int new_size, word_cnt, n, i;

	/* spi dma word must be multiple of 8byte(64bit)  */
	word_cnt = (size + 7) / 8;

	/* spi dma cycle must be multiple of word and (2~8) */
	if (word_cnt > 15) {
		for (i = 8; i > 1; i--) {
			n = word_cnt % i;
			if (n == 0)
				break;
		}
		word_cnt += n;
	}

	new_size = word_cnt * 8;
	return new_size;
}

static int ldim_spi_dump_buffer_array(unsigned char *buf, int buf_len)
{
	int i;
	int strlen;
	unsigned char *strbuf;

	LDIMPR("[%s] buf_len = %d\n", __func__, buf_len);
	strlen = 0;
	strbuf = kcalloc(100, sizeof(unsigned char), GFP_KERNEL);
	if (!strbuf)
		return -1;

	for (i = 0; i < buf_len; i++) {
		strlen += sprintf(strbuf + strlen, "0x%02x ", buf[i]);
		if (((i % 8) == 7) || (i == (buf_len - 1))) {
			pr_info("%s\n", strbuf);
			strlen = 0;
			}
	}
	pr_info("\n");
	kfree(strbuf);
	return 0;
}

static u64 _bswap64(u64 a)
{
	a = ((a & 0x00000000000000FFULL) << 56) |
	    ((a & 0x000000000000FF00ULL) << 40) |
	    ((a & 0x0000000000FF0000ULL) << 24) |
	    ((a & 0x00000000FF000000ULL) <<  8) |
	    ((a & 0x000000FF00000000ULL) >>  8) |
	    ((a & 0x0000FF0000000000ULL) >> 24) |
	    ((a & 0x00FF000000000000ULL) >> 40) |
	    ((a & 0xFF00000000000000ULL) >> 56);
	return a;
}

static int ldim_spi_buf_byte_swap_64bit(unsigned char *buf, int tlen, int xlen)
{
	u64 *tmp = (u64 *)buf;
	u64 a = 0;
	int n, i;

	if (!buf)
		return -1;

	if (ldim_debug_print & LDIM_DBG_PR_SWAP_64BIT) {
		LDIMPR("dump Original buf len = %d\n", tlen);
		ldim_spi_dump_buffer_array(buf, tlen);
	}

	for (i = tlen; i < xlen; i++)
		buf[i] = 0;
	n = xlen / 8;
	for (i = 0; i < n; i++) {
		a = tmp[i];
		tmp[i] = _bswap64(a);
	}

	if (ldim_debug_print & LDIM_DBG_PR_SWAP_64BIT) {
		LDIMPR("dump buf_swap len = %d\n", xlen);
		ldim_spi_dump_buffer_array(buf, xlen);
	}

	return 0;
}

static void ldim_spi_async_callback(void *arg)
{
	int *state = (int *)arg;

	*state = 0;
}

int ldim_spi_write_async(struct spi_device *spi, unsigned char *tbuf,
			 unsigned char *rbuf, int tlen, int max_len)
{
	struct ldim_dev_driver_s *dev_drv = dev_get_drvdata(&spi->dev);
	struct spicc_controller_data *cdata = spi->controller_data;
	struct spi_private_data *priv;
	int xlen, ret;

	if (!dev_drv || !cdata || !cdata->dirspi_async)
		return -EIO;

	priv = cdata->priv;
	if (!priv)
		return -EIO;

	xlen = tlen;
	if (spi->controller->dma_alignment) {
		spi->bits_per_word = 64;
		xlen = ldim_spi_dma_cycle_align_byte(tlen);
		if (xlen > max_len) {
			LDIMERR("spi_write_async: dma xlen %d out of max_len %d\n", xlen, max_len);
			return -1;
		}
	} else {
		spi->bits_per_word = 8;
	}

	if ((spi->controller->mode_bits & SPI_LSB_FIRST) == 0 &&
	    spi->bits_per_word == 64)
		ldim_spi_buf_byte_swap_64bit(tbuf, tlen, xlen);

	if (priv->async_busy_cnt > 5) {
		priv->async_busy = 0;
		if (ldim_debug_print & LDIM_DBG_PR_SPI)
			LDIMPR("%s: spi(0x%p) timeout!! priv->async_busy_cnt = %d\n",
			__func__, spi, priv->async_busy);
	}

	if (priv->async_busy) {
		priv->async_busy_cnt++;
		if (ldim_debug_print & LDIM_DBG_PR_SPI)
			LDIMERR("%s: spi(0x%p) async_busy=%d\n", __func__, spi, priv->async_busy);
		return -1;
	}

	priv->async_busy_cnt = 0;
	priv->async_busy = 1;
	memcpy(priv->tx_buf, tbuf, xlen * sizeof(unsigned char));//copy duty to spi tx buf
	ret = cdata->dirspi_async(spi, priv->tx_dma, priv->rx_dma, xlen,
		ldim_spi_async_callback, (void *)&priv->async_busy);
	if (ret < 0) {
		LDIMERR("%s, spi(0x%p)\n", __func__, spi);
		return -1;
	}

	return 0;
}

int ldim_spi_init_dma_trig(struct spi_device *spi)
{
	struct ldim_dev_driver_s *dev_drv = dev_get_drvdata(&spi->dev);
	struct spicc_controller_data *cdata = spi->controller_data;
	struct spi_private_data *priv;
	int ret;

	if (!cdata || !cdata->dirspi_dma_trig)
		return -EIO;

	priv = cdata->priv;
	if (!priv)
		return -EIO;

	if (is_spicc_capable(cdata, CAP_DMA_TRIG_PWM_VS) &&
		is_spicc_capable(cdata, CAP_TRIG_DELAY)) {
		cdata->dma_trig_delay = dev_drv->spi_conf.vs2cs_delay * 1000 / 6;
		ret = cdata->dirspi_dma_trig(spi, priv->tx_dma, priv->rx_dma,
			priv->xlen, DMA_TRIG_PWM_VS);
	} else if (is_spicc_capable(cdata, CAP_DMA_TRIG_VSYNC) &&
		is_spicc_capable(cdata, CAP_TRIG_DELAY)) {
		cdata->dma_trig_delay = dev_drv->spi_conf.vs2cs_delay * 1000 / 6;
		ret = cdata->dirspi_dma_trig(spi, priv->tx_dma, priv->rx_dma,
			priv->xlen, DMA_TRIG_VSYNC);
	} else if (is_spicc_capable(cdata, CAP_DMA_TRIG_LINE_N)) {
		ret = cdata->dirspi_dma_trig(spi, priv->tx_dma, priv->rx_dma,
			priv->xlen, DMA_TRIG_LINE_N);
	} else {
		LDIMERR("%s spi(0x%p) init dma trig error!\n", __func__, spi);
		ret = -1;
	}

	priv->trig_init = 1;

	if (ldim_debug_print & LDIM_DBG_PR_SPI)
		LDIMPR("%s spi(0x%p) init dma trig success!\n", __func__, spi);

	return ret;
}

int ldim_spi_dma_trig_start(struct spi_device *spi)
{
	struct ldim_dev_driver_s *dev_drv = dev_get_drvdata(&spi->dev);
	struct spicc_controller_data *cdata = spi->controller_data;
	struct spi_private_data *priv;
	int ret;

	if (!dev_drv || !cdata) {
		LDIMPR("%s, dev_drv or cdata is null, spi=0x%p\n", __func__, spi);
		return -EIO;
	}

	if (!cdata->dirspi_dma_trig_start) {
		LDIMPR("%s, cdata->dirspi_dma_trig_start is null, spi=0x%p\n", __func__, spi);
		return -EIO;
	}

	priv = cdata->priv;
	if (!priv) {
		LDIMPR("%s, priv is null,spi=0x%p\n", __func__, spi);
		return -EIO;
	}

	if (!priv->trig_data_ready) {
		LDIMPR("%s, priv->trig_data_ready=0, spi=0x%p\n", __func__, spi);
		return -EIO;
	}

	if (!priv->trig_init) {
		LDIMPR("%s, priv->trig_init=0, spi=0x%p\n", __func__, spi);
		return -EIO;
	}

	ret = cdata->dirspi_dma_trig_start(spi);

	if (ldim_debug_print & LDIM_DBG_PR_SPI)
		LDIMPR("spi(0x%p) dma trig start!\n", spi);

	return ret;
}

int ldim_spi_dma_trig_start_all(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	int i, ret = 0;

	if (!dev_drv)
		return -EIO;

	if (dev_drv->spi_conf.trig_mode == SPI_ASYNC)
		return 0;

	for (i = 0; i < dev_drv->spi_conf.spi_dev_num; i++)
		ret |= ldim_spi_dma_trig_start(dev_drv->spi_conf.spi_dev[i]);

	return ret;
}

int ldim_spi_dma_trig_stop(struct spi_device *spi)
{
	struct spicc_controller_data *cdata = spi->controller_data;
	int ret;

	if (!cdata || !cdata->dirspi_dma_trig_stop)
		return -EIO;

	ret = cdata->dirspi_dma_trig_stop(spi);

	if (ldim_debug_print & LDIM_DBG_PR_SPI)
		LDIMPR("spi(0x%p) dma trig stop!\n", spi);

	return ret;
}

int ldim_spi_dma_trig_stop_all(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	int i, ret = 0;

	if (!dev_drv)
		return -EIO;

	if (dev_drv->spi_conf.trig_mode == SPI_ASYNC)
		return 0;

	for (i = 0; i < dev_drv->spi_conf.spi_dev_num; i++)
		ret |= ldim_spi_dma_trig_stop(dev_drv->spi_conf.spi_dev[i]);

	return ret;
}

int ldim_spi_write_dma_trig(struct spi_device *spi, unsigned char *tbuf,
			 unsigned char *rbuf, int tlen, int max_len)
{
	struct ldim_dev_driver_s *dev_drv = dev_get_drvdata(&spi->dev);
	struct aml_lcd_drv_s *pdrv = aml_lcd_get_driver(0);
	struct spicc_controller_data *cdata = spi->controller_data;
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct spi_private_data *priv;
	int xlen, ret;
	int spi_trig_line, current_line, delta_line, line_left;
	int line_total_cnt;


	if (!dev_drv || !cdata) {
		LDIMPR("%s, dev_drv or cdata is null, spi=0x%p\n", __func__, spi);
		return -EIO;
	}

	priv = cdata->priv;
	if (!priv) {
		LDIMPR("%s, priv is null,spi=0x%p\n", __func__, spi);
		return -EIO;
	}

	xlen = tlen;
	if (spi->controller->dma_alignment) {
		spi->bits_per_word = 64;
		xlen = ldim_spi_dma_cycle_align_byte(tlen);
		if (xlen > max_len) {
			LDIMERR("spi_write_dma_trig: dma xlen %d out of max_len %d\n",
				xlen, max_len);
			return -1;
		}
	} else {
		spi->bits_per_word = 8;
	}

	if ((spi->controller->mode_bits & SPI_LSB_FIRST) == 0 &&
	    spi->bits_per_word == 64)
		ldim_spi_buf_byte_swap_64bit(tbuf, tlen, xlen);

	if (!cdata->dmatrig_is_busy) {
		LDIMERR("%s: cdata->dmatrig_is_busy is null\n", __func__);
		return -EIO;
	}

	spi_trig_line = dev_drv->spi_conf.vs2cs_delay;
	current_line = lcd_get_encl_line_cnt(pdrv);
	line_total_cnt = ldim_drv->trig_param.line_total_cnt;
	//vrr mode may cause current_line bigger than line_total_cnt
	if (current_line >= line_total_cnt)
		current_line = 0;
	delta_line = current_line - spi_trig_line;

	// delta_line > 0 : current frame
	// delta_line <= 0 : already in next frame
	if (delta_line > 0)
		line_left = line_total_cnt - current_line + spi_trig_line;
	else
		line_left = spi_trig_line - current_line;

	if (line_left <= SPI_SAFE_LINE_MARGIN) {
		ldim_drv->trig_param.lost_frame_cnt++;
		if (ldim_debug_print & LDIM_DBG_PR_SPI)
			LDIMERR("%s: line_total_cnt=%d, current_line=%d, line_left=%d, delta=%d\n",
				__func__, line_total_cnt, current_line, line_left, delta_line);
		return -1;
	}

	memcpy(priv->tx_buf, tbuf, xlen * sizeof(unsigned char));//copy duty to spi tx buf

	if (priv->trig_init == 0 ||
		xlen != priv->xlen) {
		priv->xlen = xlen;
		ret = ldim_spi_init_dma_trig(spi);
		ret = ldim_spi_dma_trig_start(spi);
	}

	priv->trig_data_ready = 1;

	return 0;
}

int ldim_spi_write_xfer(struct spi_device *spi, unsigned char *tbuf, int tlen)
{
	struct ldim_dev_driver_s *dev_drv = dev_get_drvdata(&spi->dev);
	struct spicc_controller_data *cdata = spi->controller_data;
	struct spi_private_data *priv;
	int ret;

	if (!dev_drv || !cdata) {
		LDIMERR("---- %s: dev_drv or cdata is null\n", __func__);
		return -EIO;
	}

	priv = cdata->priv;
	if (!priv) {
		LDIMERR("---- %s: priv is null\n", __func__);
		return -EIO;
	}

	if (priv->async_busy) {
		if (ldim_debug_print & LDIM_DBG_PR_SPI)
			LDIMERR("%s: async_busy=%d\n", __func__, priv->async_busy);
		return -1;
	}

	if (tlen > 16) {
		LDIMERR("%s: tlen=%d bigger than 16\n", __func__, tlen);
		return -1;
	}

	priv->async_busy = 1;

	spi->bits_per_word = 8;
	ret = cdata->dirspi_xfer(spi, tbuf, 0, tlen);
	if (ret)
		LDIMERR("%s spi(0x%p) failed\n", __func__, spi);
	priv->async_busy = 0;

	return ret;
}

int ldim_spi_write(struct spi_device *spi, unsigned char *tbuf, int tlen)
{
	struct ldim_dev_driver_s *dev_drv = dev_get_drvdata(&spi->dev);
	struct spicc_controller_data *cdata = spi->controller_data;
	struct spi_private_data *priv;
	struct spi_transfer xfer;
	struct spi_message msg;
	int ret;

	if (!dev_drv || !cdata) {
		LDIMERR("---- %s: dev_drv or cdata is null\n", __func__);
		return -EIO;
	}

	priv = cdata->priv;
	if (!priv) {
		LDIMERR("---- %s: priv is null\n", __func__);
		return -EIO;
	}

	if (priv->async_busy) {
		if (ldim_debug_print & LDIM_DBG_PR_SPI)
			LDIMERR("%s: spi(0x%p) async_busy=%d\n", __func__, spi, priv->async_busy);
		return -1;
	}
	priv->async_busy = 1;

	if (dev_drv->spi_conf.cs2clk_delay)
		udelay(dev_drv->spi_conf.cs2clk_delay);

	spi->bits_per_word = 8;
	spi_message_init(&msg);
	memset(&xfer, 0, sizeof(xfer));
	xfer.tx_buf = (void *)tbuf;
	xfer.rx_buf = NULL;
	xfer.len = tlen;
	spi_message_add_tail(&xfer, &msg);
	ret = spi_sync(spi, &msg);
	if (ret)
		LDIMERR("%s spi(0x%p)\n", __func__, spi);
	priv->async_busy = 0;

	return ret;
}

int ldim_spi_read(struct spi_device *spi, unsigned char *tbuf, int tlen,
		  unsigned char *rbuf, int rlen)
{
	struct ldim_dev_driver_s *dev_drv = dev_get_drvdata(&spi->dev);
	struct spicc_controller_data *cdata = spi->controller_data;
	struct spi_private_data *priv;
	struct spi_transfer xfer[2];
	struct spi_message msg;
	int ret;

	if (!dev_drv || !cdata) {
		LDIMERR("---- %s: dev_drv or cdata is null\n", __func__);
		return -EIO;
	}

	priv = cdata->priv;
	if (!priv) {
		LDIMERR("---- %s: priv is null\n", __func__);
		return -EIO;
	}

	if (priv->async_busy) {
		if (ldim_debug_print & LDIM_DBG_PR_SPI)
			LDIMERR("%s: spi(0x%p) async_busy=%d\n", __func__, spi, priv->async_busy);
		return -1;
	}
	priv->async_busy = 1;

	if (dev_drv->spi_conf.cs2clk_delay)
		udelay(dev_drv->spi_conf.cs2clk_delay);

	spi->bits_per_word = 8;
	spi_message_init(&msg);
	memset(&xfer, 0, sizeof(xfer));
	xfer[0].tx_buf = (void *)tbuf;
	xfer[0].rx_buf = NULL;
	xfer[0].len = tlen;
	spi_message_add_tail(&xfer[0], &msg);
	xfer[1].tx_buf = NULL;
	xfer[1].rx_buf = (void *)rbuf;
	xfer[1].len = rlen;
	spi_message_add_tail(&xfer[1], &msg);
	ret = spi_sync(spi, &msg);
	if (ret)
		LDIMERR("%s spi(0x%p)\n", __func__, spi);
	priv->async_busy = 0;

	return ret;
}

/* send and read buf at the same time */
int ldim_spi_read_sync(struct spi_device *spi, unsigned char *tbuf,
		       unsigned char *rbuf, int len)
{
	struct ldim_dev_driver_s *dev_drv = dev_get_drvdata(&spi->dev);
	struct spicc_controller_data *cdata = spi->controller_data;
	struct spi_private_data *priv;
	struct spi_transfer xfer;
	struct spi_message msg;
	int ret;

	if (!dev_drv || !cdata) {
		LDIMERR("---- %s: dev_drv or cdata is null\n", __func__);
		return -EIO;
	}

	priv = cdata->priv;
	if (!priv) {
		LDIMERR("---- %s: priv is null\n", __func__);
		return -EIO;
	}

	if (priv->async_busy) {
		if (ldim_debug_print & LDIM_DBG_PR_SPI)
			LDIMERR("%s: spi(0x%p) async_busy=%d\n", __func__, spi, priv->async_busy);
		return -1;
	}
	priv->async_busy = 1;

	if (dev_drv->spi_conf.cs2clk_delay)
		udelay(dev_drv->spi_conf.cs2clk_delay);

	spi->bits_per_word = 8;
	spi_message_init(&msg);
	memset(&xfer, 0, sizeof(xfer));
	xfer.tx_buf = (void *)tbuf;
	xfer.rx_buf = (void *)rbuf;
	xfer.len = len;
	spi_message_add_tail(&xfer, &msg);

	ret = spi_sync(spi, &msg);
	if (ret)
		LDIMERR("%s spi(0x%p)\n", __func__, spi);
	priv->async_busy = 0;

	return ret;
}

void ldim_spi_async_busy_clear(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_dev_driver_s *dev_drv;
	struct spi_device *spi;
	struct spicc_controller_data *cdata;
	struct spi_private_data *priv;
	int i;

	if (!ldim_drv) {
		LDIMPR("%s: ldim_drv is null,return!\n", __func__);
		return;
	}

	dev_drv = ldim_drv->dev_drv;
	if (!dev_drv) {
		LDIMPR("%s: dev_drv is null,return!\n", __func__);
		return;
	}

	for (i = 0; i < dev_drv->spi_conf.spi_dev_num; i++) {
		spi = dev_drv->spi_conf.spi_dev[i];
		if (!spi) {
			LDIMPR("%s: spi(0x%p) is null,return!\n", __func__, spi);
			return;
		}

		cdata = spi->controller_data;
		if (!cdata) {
			LDIMPR("%s: cdata is null,return!\n", __func__);
			return;
		}

		priv = cdata->priv;
		if (!priv) {
			LDIMPR("%s: spi_priv is null,return!\n", __func__);
			return;
		}

		priv->async_busy = 0;
		priv->async_busy_cnt = 0;
	}
}

void ldim_spi_async_busy_proc(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_dev_driver_s *dev_drv;
	struct spi_device *spi;
	struct spicc_controller_data *cdata;
	struct spi_private_data *priv;
	int i;

	if (!ldim_drv) {
		LDIMPR("%s: ldim_drv is null,return!\n", __func__);
		return;
	}

	dev_drv = ldim_drv->dev_drv;
	if (!dev_drv) {
		LDIMPR("%s: dev_drv is null,return!\n", __func__);
		return;
	}

	for (i = 0; i < dev_drv->spi_conf.spi_dev_num; i++) {
		spi = dev_drv->spi_conf.spi_dev[i];
		if (!spi) {
			LDIMPR("%s: spi(0x%p) is null,return!\n", __func__, spi);
			return;
		}

		cdata = spi->controller_data;
		if (!cdata) {
			LDIMPR("%s: cdata is null,return!\n", __func__);
			return;
		}

		priv = cdata->priv;
		if (!priv) {
			LDIMPR("%s: priv is null,return!\n", __func__);
			return;
		}

		if (priv->async_busy) {
			if (cdata->dirspi_busy_proc)
				cdata->dirspi_busy_proc(spi);
		}
	}
}

static int ldim_spi_dev_probe(struct spi_device *spi)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	struct spicc_controller_data *cdata = spi->controller_data;
	struct spi_private_data *priv;
	int ret = 0;

	if (!dev_drv || !cdata)
		return 0;

	priv = cdata->priv;
	if (!priv)
		return 0;

	dev_set_drvdata(&spi->dev, dev_drv);
	if (spi->controller->dma_alignment)
		spi->bits_per_word = 64;
	else
		spi->bits_per_word = 8;

	if (spi->controller->mode_bits & SPI_LSB_FIRST)
		spi->mode |= SPI_LSB_FIRST;

	spi->cs_setup.value = dev_drv->spi_conf.cs2clk_delay;
	spi->cs_hold.value = dev_drv->spi_conf.clk2cs_delay;
	ret = spi_setup(spi);
	if (ret)
		LDIMERR("spi setup failed\n");

	priv->async_busy = 0;
	priv->async_busy_cnt = 0;
	priv->trig_init = 0;
	priv->trig_data_ready = 0;
	return ret;
}

static void ldim_spi_dev_remove(struct spi_device *spi)
{
	struct ldim_dev_driver_s *dev_drv = dev_get_drvdata(&spi->dev);
	struct device *dev = spi->controller->dev.parent;
	struct spicc_controller_data *cdata = spi->controller_data;
	struct spi_private_data *priv;

	if (!dev_drv || !cdata)
		return;

	priv = cdata->priv;
	if (!priv)
		return;

	if (dev_drv->spi_conf.trig_mode != SPI_ASYNC)  {
		dma_free_coherent(dev, priv->xlen, priv->tx_buf, priv->tx_dma);
		dma_free_coherent(dev, priv->xlen, priv->rx_buf, priv->rx_dma);
	}

	dev_drv->spi_conf.spi_dev[priv->dev_idx] = NULL;
	dev_set_drvdata(&spi->dev, NULL);

	return;
}

static struct spi_driver ldim_spi_dev_driver = {
	.probe = ldim_spi_dev_probe,
	.remove = ldim_spi_dev_remove,
	.driver = {
		.name = "ldim_dev",
		.owner = THIS_MODULE,
	},
};

struct spi_controller *spi_aliases_to_ctlr(const char *spi_aliases, int spicc_version)
{
	struct platform_device *pdev;
	struct spi_controller *ctlr = NULL;
	struct device_node *of_aliases;
	struct device_node *target_node;
	struct property *pp;
	const char *alias_name;
	struct meson_spicc_device *spicc_v1_device_ = NULL;
	struct spicc_device *spicc_v2_device_ = NULL;

	of_aliases = of_find_node_by_path("/aliases");

	if (!of_aliases) {
		pr_err("Failed to find /aliases node\n");
		return NULL;
	}

	for_each_property_of_node(of_aliases, pp) {
		alias_name = pp->name;

		if (!strncmp(alias_name, spi_aliases, strlen(pp->name))) {
			target_node = of_find_node_by_path(pp->value);

			if (!target_node) {
				pr_warn("Alias %s has no valid target node\n", alias_name);
				continue;
			}

			pdev = of_find_device_by_node(target_node);
			if (!pdev)
				return NULL;

			if (spicc_version == SPICC_V1) {
				spicc_v1_device_ = platform_get_drvdata(pdev);
				ctlr = spicc_v1_device_->controller;
			} else if (spicc_version == SPICC_V2) {
				spicc_v2_device_ = platform_get_drvdata(pdev);
				ctlr = spicc_v2_device_->controller;
			}

			of_node_put(target_node);
		}
	}

	of_node_put(of_aliases);

	return ctlr;
}

int ldim_spi_driver_add(struct ldim_dev_driver_s *dev_drv)
{
	struct spi_controller *ctlr;
	struct spicc_controller_data *cdata;
	struct spi_device *spi_device;
	struct spi_conf_s *spi_conf = &dev_drv->spi_conf;
	int ret, i;
	char busname[16];

	for (i = 0; i < spi_conf->spi_dev_num; i++) {
		cdata = spi_conf->spi_info[i].controller_data;
		cdata->use_ctrl_cs = spi_conf->use_ctrl_cs;
		cdata->ss_leading_gap = spi_conf->cs2clk_delay;
		cdata->ss_trailing_gap = spi_conf->clk2cs_delay;

		snprintf(busname, sizeof(busname), "spi%d", spi_conf->spi_info[i].bus_num);
		//Default:SPICC_V2 new spicc controller after A4
		//TODO: add spicc version detect code for dts，spicltr_version = <2>;
		ctlr = spi_aliases_to_ctlr(busname, SPICC_V2);
		if (!ctlr) {
			LDIMERR("get busnum[%d] busname[%s] failed\n", i, busname);
			return -1;
		}

		spi_device = spi_new_device(ctlr, &spi_conf->spi_info[i]);
		if (!spi_device) {
			LDIMERR("get spi_device %d failed\n", i);
			return -1;
		}
		spi_conf->spi_dev[i] = spi_device;
		LDIMPR("spi_driver_add: spi_dev[%d] = 0x%p\n", i, spi_conf->spi_dev[i]);
	}

	ret = spi_register_driver(&ldim_spi_dev_driver);
	if (ret) {
		LDIMERR("%s spi_dev failed\n", __func__);
		return -1;
	}

	dev_drv->dev_transmit_start = ldim_spi_dma_trig_start_all;
	dev_drv->dev_transmit_stop = ldim_spi_dma_trig_stop_all;

	LDIMPR("%s ok\n", __func__);
	return 0;
}

int ldim_spi_driver_remove(struct ldim_dev_driver_s *dev_drv)
{
	int i;

	for (i = 0; i < dev_drv->spi_conf.spi_dev_num; i++) {
		if (dev_drv->spi_conf.spi_dev[i])
			spi_unregister_driver(&ldim_spi_dev_driver);
	}

	LDIMPR("%s ok\n", __func__);

	return 0;
}

