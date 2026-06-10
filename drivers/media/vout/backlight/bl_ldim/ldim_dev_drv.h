/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __LDIM_DEV_DRV_H_
#define __LDIM_DEV_DRV_H_

#include <linux/spi/spi.h>
#include <linux/amlogic/media/vout/backlight/aml_ldim.h>
#include "../lcd_bl.h"

extern struct bl_gpio_s ldim_gpio[BL_GPIO_NUM_MAX];
int ldim_spi_dma_cycle_align_byte(int size);

/* ldim spi api*/
int ldim_spi_init_dma_trig(struct spi_device *spi);
int ldim_spi_write_dma_trig(struct spi_device *spi, unsigned char *tbuf,
			 unsigned char *rbuf, int tlen, int max_len);
int ldim_spi_dma_trig_start(struct spi_device *spi);
int ldim_spi_dma_trig_start_all(void);
int ldim_spi_dma_trig_stop(struct spi_device *spi);
int ldim_spi_dma_trig_stop_all(void);
int ldim_spi_write_async(struct spi_device *spi, unsigned char *tbuf,
			 unsigned char *rbuf, int tlen, int max_len);
int ldim_spi_write_xfer(struct spi_device *spi, unsigned char *tbuf, int tlen);
int ldim_spi_write(struct spi_device *spi, unsigned char *tbuf, int tlen);
int ldim_spi_read(struct spi_device *spi, unsigned char *tbuf, int wlen,
		  unsigned char *rbuf, int rlen);
int ldim_spi_read_sync(struct spi_device *spi, unsigned char *tbuf,
		       unsigned char *rbuf, int len);
void ldim_spi_async_busy_proc(void);
void ldim_spi_async_busy_clear(void);
int ldim_spi_driver_add(struct ldim_dev_driver_s *dev_drv);
int ldim_spi_driver_remove(struct ldim_dev_driver_s *dev_drv);

/* ldim global api */
void ldim_gpio_probe(struct ldim_dev_driver_s *dev_drv, int index);
void ldim_gpio_set(struct ldim_dev_driver_s *dev_drv, int index, int value);
unsigned int ldim_gpio_get(struct ldim_dev_driver_s *dev_drv, int index);
void ldim_set_duty_pwm(struct bl_pwm_config_s *ld_pwm);
void ldim_pwm_off(struct bl_pwm_config_s *ld_pwm);

int ldim_dev_get_config(struct ldim_dev_driver_s *dev_drv, struct device_node *np, int index);

/* ldim dev api */
int ldim_dev_iw7027_probe(struct aml_ldim_driver_s *ldim_drv);
int ldim_dev_iw7027_remove(struct aml_ldim_driver_s *ldim_drv);

int ldim_dev_iw7039_probe(struct aml_ldim_driver_s *ldim_drv);
int ldim_dev_iw7039_remove(struct aml_ldim_driver_s *ldim_drv);

int ldim_dev_blmcu_probe(struct aml_ldim_driver_s *ldim_drv);
int ldim_dev_blmcu_remove(struct aml_ldim_driver_s *ldim_drv);

int ldim_dev_ob3350_probe(struct aml_ldim_driver_s *ldim_drv);
int ldim_dev_ob3350_remove(struct aml_ldim_driver_s *ldim_drv);

int ldim_dev_global_probe(struct aml_ldim_driver_s *ldim_drv);
int ldim_dev_global_remove(struct aml_ldim_driver_s *ldim_drv);

int ldim_dev_rgb_probe(struct aml_ldim_driver_s *ldim_drv);
int ldim_dev_rgb_remove(struct aml_ldim_driver_s *ldim_drv);

#ifdef CONFIG_AMLOGIC_BL_LDIM_ABCON
int ldim_abcon_dev_probe(struct aml_ldim_driver_s *ldim_drv);
int ldim_abcon_dev_remove(struct aml_ldim_driver_s *ldim_drv);
int ldim_dev_abcon_spb_probe(struct aml_ldim_driver_s *ldim_drv);
int ldim_dev_abcon_spb_remove(struct aml_ldim_driver_s *ldim_drv);
int ldim_dev_abcon_ospb_probe(struct aml_ldim_driver_s *ldim_drv);
int ldim_dev_abcon_ospb_remove(struct aml_ldim_driver_s *ldim_drv);
int ldim_dev_abcon_xwire_probe(struct aml_ldim_driver_s *ldim_drv);
int ldim_dev_abcon_xwire_remove(struct aml_ldim_driver_s *ldim_drv);
int ldim_dev_abcon_owc_probe(struct aml_ldim_driver_s *ldim_drv);
int ldim_dev_abcon_owc_remove(struct aml_ldim_driver_s *ldim_drv);
#endif

#endif /* __LDIM_DEV_DRV_H */
