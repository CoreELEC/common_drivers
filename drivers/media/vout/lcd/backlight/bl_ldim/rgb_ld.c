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
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/notifier.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/aml_spi.h>
#include "../../lcd_common.h"
#include "ldim_drv.h"
#include "ldim_dev_drv.h"

#define NORMAL_MSG      (0 << 7)
#define BROADCAST_MSG   BIT(7)
#define BLOCK_DATA      (0 << 6)
#define SINGLE_DATA     BIT(6)
#define rgb_CLASS_NAME "rgb"

#define VSYNC_INFO_FREQUENT        300

static DEFINE_MUTEX(spi_mutex);
static DEFINE_MUTEX(dev_mutex);

struct rgb_conf_s {
	unsigned short header;
	unsigned short cmd[3];
	unsigned short glb_bl;
	unsigned short ave[3];
	unsigned short max[3];
	unsigned short min[3];
	unsigned short test_duty[3];
	unsigned short checksum[3];
	unsigned short iset[3];
};

static struct rgb_conf_s rgb_conf = {
	.header = 0x0a55,
	.cmd = {0, 0, 0},
	.glb_bl = 0,
	.ave = {0, 0, 0},
	.max = {0, 0, 0},
	.min = {0, 0, 0},
	.test_duty = {0xfed, 0xfed, 0xfed},
	.checksum = {0xffff, 0xffff, 0xffff},
	//.iset = {205, 172, 131},//{277, 191, 127},//{205, 172, 131},//{197, 162, 135}
	.iset = {41, 54, 28},
};

struct rgb_s {
	struct spi_device *spi[3];
	unsigned char pkt_type;
	unsigned int dev_on_flag;
	unsigned short vsync_cnt;
	unsigned int rbuf_size;
	unsigned int tbuf_size;
	unsigned char header_cnt;
	unsigned char *header_data;
	unsigned char extend_cnt;
	unsigned char *extend_data;
	unsigned char adim_width;
	unsigned char pdim_width;
	unsigned short adim;	/*1byte 0x4d 30%duty */
	unsigned short pdim;	/*1byte 0xff */
	int *idim; /*current setting for each zone*/
	unsigned char datawidth;	/*0:8bit, 1:12bit, 2:16bit */
	unsigned char ext_len;
	unsigned char bl_pwm_en;	/*0:default uboot have pwm, 1: uboot without pwm*/
	unsigned int apl;
	unsigned int ch_size;
	unsigned int zone_num;
	unsigned char *rbuf[3];
	unsigned char *tbuf[3];
	unsigned char test_flag;
};

struct rgb_s *rgb;

static int rgb_hw_init_on(struct ldim_dev_driver_s *dev_drv)
{
	unsigned char chip_id;// reg, temp[2];
	//int i, retry_cnt = 0;

	chip_id = 0x01;

	/* step 1: system power_on */
	ldim_gpio_set(dev_drv, dev_drv->en_gpio, dev_drv->en_gpio_on);

	/* step 2: delay for internal logic stable */
	lcd_delay_ms(dev_drv->hw_on_delay);

	/* step 3: Generate external VSYNC to VSYNC/PWM pin */
	ldim_set_duty_pwm(&dev_drv->ldim_pwm_config);
	ldim_set_duty_pwm(&dev_drv->analog_pwm_config);
	dev_drv->pinmux_ctrl(dev_drv, 1);

	/* step 4: delay for system clock and light bar PSU stable */
	//lcd_delay_ms(520);

	return 0;
}

static int rgb_hw_init_off(struct ldim_dev_driver_s *dev_drv)
{
	ldim_gpio_set(dev_drv, dev_drv->en_gpio, dev_drv->en_gpio_off);
	dev_drv->pinmux_ctrl(dev_drv, 0);
	ldim_pwm_off(&dev_drv->ldim_pwm_config);
	ldim_pwm_off(&dev_drv->analog_pwm_config);

	return 0;
}

static void ldim_vs_debug_info(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	unsigned int i, j;

	if (rgb->vsync_cnt) //300 vsync print once
		return;
	if ((ldim_debug_print & LDIM_DBG_PR_DEV_DBG_INFO) == 0)
		return;

	for (j = 0; j < dev_drv->spi_conf.spi_dev_num; j++) {
		for (i = 0; i < rgb->tbuf_size; i++)
			LDIMPR("tbuf[%d]: 0x%x\n", i, rgb->tbuf[j][i]);
	}

	pr_info("\n");
}

static inline void ldim_data_mapping_rgb(struct aml_ldim_driver_s *ldim_drv,
	unsigned int *duty_buf, unsigned short *bl_mapping)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	unsigned int i, j, k;
	unsigned char header_cnt = 0;
	unsigned int zone_num = rgb->zone_num / 2;
	unsigned short checksum = 0;
	unsigned char *rbuf = rgb->rbuf[0];
	unsigned char *tbuf = rgb->tbuf[0];

	/*for 1st spi*/
	j = 0;
	for (i = 0; i < zone_num; i++) {
		if (rgb->test_flag) {
			rbuf[j + 1] = rgb_conf.test_duty[0] & 0xff;
			rbuf[j] = (rgb_conf.test_duty[0] >> 8) & 0xff;
			rbuf[j + 3] = rgb_conf.test_duty[1] & 0xff;
			rbuf[j + 2] = (rgb_conf.test_duty[1] >> 8) & 0xff;
			rbuf[j + 5] = rgb_conf.test_duty[2] & 0xff;
			rbuf[j + 4] = (rgb_conf.test_duty[2] >> 8) & 0xff;

		} else {
			rbuf[j + 1] = duty_buf[i] & 0xff;
			rbuf[j] = (duty_buf[i] >> 8) & 0xff;
			rbuf[j + 3] = duty_buf[i + rgb->zone_num] & 0xff;
			rbuf[j + 2] = (duty_buf[i + rgb->zone_num] >> 8) & 0xff;
			rbuf[j + 5] = duty_buf[i + 2 * rgb->zone_num] & 0xff;
			rbuf[j + 4] = (duty_buf[i + 2 * rgb->zone_num] >> 8) & 0xff;
		}
		j += 6;
	}

	k = zone_num * 3 * 2;

	header_cnt = 14 * 2;
	tbuf[0] = rgb_conf.header >> 8;
	tbuf[1] = rgb_conf.header & 0xff;
	tbuf[2] = (rgb_conf.cmd[0] >> 8) & 0xff;
	tbuf[3] = rgb_conf.cmd[0] & 0xff;
	tbuf[4] = (rgb_conf.cmd[1] >> 8) & 0xff;
	tbuf[5] = rgb_conf.cmd[1] & 0xff;
	tbuf[6] = (rgb_conf.cmd[2] >> 8) & 0xff;
	tbuf[7] = rgb_conf.cmd[2] & 0xff;
	tbuf[8] = (rgb_conf.glb_bl >> 8) & 0xff;
	tbuf[9] = rgb_conf.glb_bl & 0xff;
	tbuf[10] = (rgb_conf.ave[0] >> 8) & 0xff;
	tbuf[11] = rgb_conf.ave[0] & 0xff;
	tbuf[12] = (rgb_conf.ave[1] >> 8) & 0xff;
	tbuf[13] = rgb_conf.ave[1] & 0xff;
	tbuf[14] = (rgb_conf.ave[2] >> 8) & 0xff;
	tbuf[15] = rgb_conf.ave[2] & 0xff;
	tbuf[16] = (rgb_conf.max[0] >> 8) & 0xff;
	tbuf[17] = rgb_conf.max[0] & 0xff;
	tbuf[18] = (rgb_conf.max[1] >> 8) & 0xff;
	tbuf[19] = rgb_conf.max[1] & 0xff;
	tbuf[20] = (rgb_conf.max[2] >> 8) & 0xff;
	tbuf[21] = rgb_conf.max[2] & 0xff;
	tbuf[22] = (rgb_conf.min[0] >> 8) & 0xff;
	tbuf[23] = rgb_conf.min[0] & 0xff;
	tbuf[24] = (rgb_conf.min[1] >> 8) & 0xff;
	tbuf[25] = rgb_conf.min[1] & 0xff;
	tbuf[26] = (rgb_conf.min[2] >> 8) & 0xff;
	tbuf[27] = rgb_conf.min[2] & 0xff;

	for (i = 0; i < k; i++)
		tbuf[i + header_cnt] = rbuf[i];

	k = zone_num * 3 + 14;
	j = 0;
	for (i = 0; i < k; i++) {
		checksum ^= (tbuf[j + 1] + (tbuf[j] << 8));
		j += 2;
	}

	if (rgb_conf.checksum[0] != 0xffff)
		checksum = rgb_conf.checksum[0];

	k = (zone_num * 3 + 14) * 2;
	tbuf[k + 1] = checksum;
	tbuf[k] = checksum >> 8;

	/*for 2nd spi*/
	/* here For reference only, need to be set according to the actual device spec!!!*/
	if (dev_drv->spi_conf.spi_dev_num < 2)
		return;
	rbuf = rgb->rbuf[1];
	tbuf = rgb->tbuf[1];

	header_cnt = 0;
	checksum = 0;
	j = 0;
	for (i = 0; i < zone_num; i++) {
		if (rgb->test_flag) {
			rbuf[j + 1] = rgb_conf.test_duty[0] & 0xff;
			rbuf[j] = (rgb_conf.test_duty[0] >> 8) & 0xff;
			rbuf[j + 3] = rgb_conf.test_duty[1] & 0xff;
			rbuf[j + 2] = (rgb_conf.test_duty[1] >> 8) & 0xff;
			rbuf[j + 5] = rgb_conf.test_duty[2] & 0xff;
			rbuf[j + 4] = (rgb_conf.test_duty[2] >> 8) & 0xff;

		} else {
			rbuf[j + 1] = duty_buf[i + zone_num] & 0xff;
			rbuf[j] = (duty_buf[i + zone_num] >> 8) & 0xff;
			rbuf[j + 3] = duty_buf[i + zone_num + rgb->zone_num] & 0xff;
			rbuf[j + 2] = (duty_buf[i + zone_num + rgb->zone_num] >> 8) & 0xff;
			rbuf[j + 5] = duty_buf[i + zone_num + 2 * rgb->zone_num] & 0xff;
			rbuf[j + 4] = (duty_buf[i + zone_num + 2 * rgb->zone_num] >> 8) & 0xff;
		}
		j += 6;
	}

	k = zone_num * 3 * 2;

	header_cnt = 14 * 2;
	tbuf[0] = rgb_conf.header >> 8;
	tbuf[1] = rgb_conf.header & 0xff;
	tbuf[2] = (rgb_conf.cmd[0] >> 8) & 0xff;
	tbuf[3] = rgb_conf.cmd[0] & 0xff;
	tbuf[4] = (rgb_conf.cmd[1] >> 8) & 0xff;
	tbuf[5] = rgb_conf.cmd[1] & 0xff;
	tbuf[6] = (rgb_conf.cmd[2] >> 8) & 0xff;
	tbuf[7] = rgb_conf.cmd[2] & 0xff;
	tbuf[8] = (rgb_conf.glb_bl >> 8) & 0xff;
	tbuf[9] = rgb_conf.glb_bl & 0xff;
	tbuf[10] = (rgb_conf.ave[0] >> 8) & 0xff;
	tbuf[11] = rgb_conf.ave[0] & 0xff;
	tbuf[12] = (rgb_conf.ave[1] >> 8) & 0xff;
	tbuf[13] = rgb_conf.ave[1] & 0xff;
	tbuf[14] = (rgb_conf.ave[2] >> 8) & 0xff;
	tbuf[15] = rgb_conf.ave[2] & 0xff;
	tbuf[16] = (rgb_conf.max[0] >> 8) & 0xff;
	tbuf[17] = rgb_conf.max[0] & 0xff;
	tbuf[18] = (rgb_conf.max[1] >> 8) & 0xff;
	tbuf[19] = rgb_conf.max[1] & 0xff;
	tbuf[20] = (rgb_conf.max[2] >> 8) & 0xff;
	tbuf[21] = rgb_conf.max[2] & 0xff;
	tbuf[22] = (rgb_conf.min[0] >> 8) & 0xff;
	tbuf[23] = rgb_conf.min[0] & 0xff;
	tbuf[24] = (rgb_conf.min[1] >> 8) & 0xff;
	tbuf[25] = rgb_conf.min[1] & 0xff;
	tbuf[26] = (rgb_conf.min[2] >> 8) & 0xff;
	tbuf[27] = rgb_conf.min[2] & 0xff;

	for (i = 0; i < k; i++)
		tbuf[i + header_cnt] = rbuf[i];

	k = zone_num * 3 + 14;
	j = 0;
	for (i = 0; i < k; i++) {
		checksum ^= (tbuf[j + 1] + (tbuf[j] << 8));
		j += 2;
	}

	if (rgb_conf.checksum[0] != 0xffff)
		checksum = rgb_conf.checksum[0];

	k = (zone_num * 3 + 14) * 2;
	tbuf[k + 1] = checksum;
	tbuf[k] = checksum >> 8;
}

unsigned char CRC8Calcurate(unsigned char *p, char counter)
{
	unsigned char crc8 = 0;

	for ( ; counter > 0; counter--) {
		crc8 ^= *p;
		p++;
	}

	return(crc8);
}

static inline void ldim_data_mapping_rgb_1(struct aml_ldim_driver_s *ldim_drv,
	unsigned int *duty_buf, unsigned short *bl_mapping)
{
	unsigned int i, j, k;
	unsigned char header_cnt = 0;
	unsigned int zone_num = rgb->zone_num;
	unsigned char checksum = 0;
	unsigned char *rbuf = rgb->rbuf[0];
	unsigned char *tbuf = rgb->tbuf[0];

	/*for 1st spi*/
	j = 0;
	for (i = 0; i < zone_num; i++) {
		if (rgb->test_flag) {
			rbuf[j + 1] = rgb_conf.test_duty[0] & 0xff;
			rbuf[j] = (rgb_conf.test_duty[0] >> 8) & 0xff;
			rbuf[j + 3] = rgb_conf.test_duty[1] & 0xff;
			rbuf[j + 2] = (rgb_conf.test_duty[1] >> 8) & 0xff;
			rbuf[j + 5] = rgb_conf.test_duty[2] & 0xff;
			rbuf[j + 4] = (rgb_conf.test_duty[2] >> 8) & 0xff;

		} else {
			rbuf[j + 1] = (duty_buf[i] << 4) & 0xff;
			rbuf[j] = ((duty_buf[i] << 4) >> 8) & 0xff;
			rbuf[j + 3] = (duty_buf[i + rgb->zone_num] << 4) & 0xff;
			rbuf[j + 2] = ((duty_buf[i + rgb->zone_num] << 4) >> 8) & 0xff;
			rbuf[j + 5] = (duty_buf[i + 2 * rgb->zone_num] << 4) & 0xff;
			rbuf[j + 4] = ((duty_buf[i + 2 * rgb->zone_num] << 4) >> 8) & 0xff;
		}
		j += 6;
	}

	k = zone_num * 3 * 2;

	header_cnt = 4;
	tbuf[0] = 0xaa;
	tbuf[1] = 0x81;
	tbuf[2] = 0xff;
	tbuf[3] = 0xff;

	for (i = 0; i < k; i++)
		tbuf[i + header_cnt] = rbuf[i];

	k = zone_num * 6 + 4;
	checksum = CRC8Calcurate(tbuf, k);

	tbuf[k] = checksum;
}

unsigned short CRC16Calcurate(unsigned short *p, unsigned int counter)
{
	unsigned short crc16 = 0;

	for ( ; counter > 0; counter--) {
		crc16 ^= *p;
		p++;
	}

	return(crc16);
}

static inline void ldim_data_mapping_rgb_2(struct aml_ldim_driver_s *ldim_drv,
	unsigned int *duty_buf, unsigned short *bl_mapping)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	unsigned int i, j, k, idx;
	unsigned char header_cnt = 0;
	unsigned int zone_num = rgb->zone_num;
	unsigned short checksum = 0;

	for (idx = 0; idx < dev_drv->spi_conf.spi_dev_num; idx++) {
		j = 0;
		for (i = 0; i < zone_num; i++) {
			if (rgb->test_flag) {
				rgb->rbuf[idx][j + 1] = rgb_conf.iset[idx]  & 0xff;
				rgb->rbuf[idx][j] = (rgb_conf.iset[idx] >> 8) & 0xff;
				rgb->rbuf[idx][j + 3] = rgb_conf.test_duty[idx] & 0xff;
				rgb->rbuf[idx][j + 2] = (rgb_conf.test_duty[idx] >> 8) & 0xff;
			} else {
				rgb->rbuf[idx][j + 1] = rgb_conf.iset[idx] & 0xff;
				rgb->rbuf[idx][j] = (rgb_conf.iset[idx] >> 8) & 0xff;
				rgb->rbuf[idx][j + 3] =
					(duty_buf[i + idx * rgb->zone_num] << 4) & 0xff;
				rgb->rbuf[idx][j + 2] =
					((duty_buf[i + idx * rgb->zone_num] << 4) >> 8) & 0xff;
			}
			j += 4;
		}

		k = zone_num * 4;

		header_cnt = 4;
		rgb->tbuf[idx][0] = 0xaa + idx;
		rgb->tbuf[idx][1] = 0x55;
		rgb->tbuf[idx][2] = 0x00;
		rgb->tbuf[idx][3] = 0x00;

		for (i = 0; i < k; i++)
			rgb->tbuf[idx][i + header_cnt] = rgb->rbuf[idx][i];

		k = zone_num * 4 + 4;
		checksum = CRC16Calcurate((unsigned short *)rgb->tbuf[idx], k / 2);

		rgb->tbuf[idx][k + 1] = (checksum >> 8) & 0xff;
		rgb->tbuf[idx][k] = checksum & 0xff;
	}
}

static inline void ldim_data_mapping_rgb_3(struct aml_ldim_driver_s *ldim_drv,
	unsigned int *duty_buf, unsigned short *bl_mapping)
{
	unsigned int i, j, k;
	unsigned int zone_num = rgb->zone_num;
	unsigned char *rbuf = rgb->rbuf[0];
	unsigned char *tbuf = rgb->tbuf[0];
	//current value from customer
	unsigned char cur_r = rgb_conf.iset[0];//0x29;
	unsigned char cur_g = rgb_conf.iset[1];//0x36;
	unsigned char cur_b = rgb_conf.iset[2];//0x1c;

	/*for 1st spi*/
	j = 0;
	for (i = 0; i < zone_num; i++) {
		if (rgb->test_flag) {
			//R
			rbuf[j + 1] = (rgb_conf.test_duty[0] >> 2) & 0xff;
			rbuf[j] = ((cur_r & 0x3f) << 2) | ((rgb_conf.test_duty[0] >> 10) & 0x3);
			//G
			rbuf[j + 3] = (rgb_conf.test_duty[1] >> 2) & 0xff;
			rbuf[j + 2] = ((cur_g & 0x3f) << 2) | ((rgb_conf.test_duty[1] >> 10) & 0x3);
			//B
			rbuf[j + 5] = (rgb_conf.test_duty[2] >> 2) & 0xff;
			rbuf[j + 4] = ((cur_b & 0x3f) << 2) | ((rgb_conf.test_duty[2] >> 10) & 0x3);
		} else {
			//R duty_buf 12bit valid data, use bits[11:2]
			rbuf[j + 1] = (duty_buf[i] >> 2) & 0xff;
			rbuf[j] = ((cur_r & 0x3f) << 2) | ((duty_buf[i] >> 10) & 0x3);
			//G
			rbuf[j + 3] = (duty_buf[i + rgb->zone_num] >> 2) & 0xff;
			rbuf[j + 2] =
				((cur_g & 0x3f) << 2) | ((duty_buf[i + rgb->zone_num] >> 10) & 0x3);
			//B
			rbuf[j + 5] = (duty_buf[i + 2 * rgb->zone_num] >> 2) & 0xff;
			rbuf[j + 4] = ((cur_b & 0x3f) << 2) |
				((duty_buf[i + 2 * rgb->zone_num] >> 10) & 0x3);
		}
		j += 6;
	}

	k = zone_num * 3 * 2;
	for (i = 0; i < k; i++)
		tbuf[i] = rbuf[i];
}

static int rgb_transmit(struct aml_ldim_driver_s *ldim_drv, unsigned int *buf,
		      unsigned int len)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	int ret = 0;
	int j;

	if (!rgb)
		return -1;

	if (rgb->vsync_cnt++ >= VSYNC_INFO_FREQUENT)
		rgb->vsync_cnt = 0;

	if (rgb->dev_on_flag == 0) {
		if (rgb->vsync_cnt == 0)
			LDIMPR("on_flag=%d\n", rgb->dev_on_flag);
		return 0;
	}

	if (rgb->pkt_type == 0)
		ldim_data_mapping_rgb(ldim_drv, buf, dev_drv->bl_mapping);
	else if (rgb->pkt_type == 1)
		ldim_data_mapping_rgb_1(ldim_drv, buf, dev_drv->bl_mapping);
	else if (rgb->pkt_type == 2)
		ldim_data_mapping_rgb_2(ldim_drv, buf, dev_drv->bl_mapping);
	else if (rgb->pkt_type == 3)
		ldim_data_mapping_rgb_3(ldim_drv, buf, dev_drv->bl_mapping);

	ldim_vs_debug_info(ldim_drv);

	switch (dev_drv->spi_conf.trig_mode) {
	case SPI_ASYNC:
		for (j = 0; j < dev_drv->spi_conf.spi_dev_num; j++)
			ret |=  ldim_spi_write_async(rgb->spi[j], rgb->tbuf[j], rgb->rbuf[j],
		rgb->tbuf_size, rgb->tbuf_size);
		break;
	default:
		for (j = 0; j < dev_drv->spi_conf.spi_dev_num; j++)
			ret |= ldim_spi_write_dma_trig(rgb->spi[j], rgb->tbuf[j], rgb->rbuf[j],
		rgb->tbuf_size, rgb->tbuf_size);
		break;
	}
	return ret;
}

static int rgb_transmit_dummy(struct aml_ldim_driver_s *ldim_drv)
{
	return 0;
}

static int rgb_fb_handle(struct aml_ldim_driver_s *ldim_drv)
{
	int ret = 0;
	return ret;
}

static int rgb_config_update(struct aml_ldim_driver_s *ldim_drv)
{
	int ret = 0;

	return ret;
}

static int rgb_power_on(struct aml_ldim_driver_s *ldim_drv)
{
	if (!rgb)
		return -1;
	if (rgb->dev_on_flag) {
		LDIMPR("rgb is already on, exit\n");
		return 0;
	}

	mutex_lock(&dev_mutex);
	rgb_hw_init_on(ldim_drv->dev_drv);
	rgb->dev_on_flag = 1;
	rgb->vsync_cnt = 0;
	mutex_unlock(&dev_mutex);

	LDIMPR("rgb power on done\n");
	return 0;
}

static int rgb_power_off(struct aml_ldim_driver_s *ldim_drv)
{
	if (!rgb)
		return -1;

	mutex_lock(&dev_mutex);
	rgb->dev_on_flag = 0;
	rgb_hw_init_off(ldim_drv->dev_drv);
	ldim_spi_async_busy_clear();
	mutex_unlock(&dev_mutex);

	LDIMPR("rgb power off done\n");
	return 0;
}

static ssize_t rgb_show(const struct class *class, const struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_dev_driver_s *dev_drv;
	int len = 0;

	if (!rgb)
		return sprintf(buf, "rgb is null\n");
	dev_drv = ldim_drv->dev_drv;

	if (!attr->attr.name)
		return -1;

	if (!strcmp(attr->attr.name, "rgb_status")) {
		len = sprintf(buf, "rgb status:\n"
				"dev_index      = %d\n"
				"on_flag        = %d\n"
				"vsync_cnt      = %d\n"
				"spi_dev_num      = %d\n"
				"spi			= 0x%p\n"
				"tbuf_size      = %d\n"
				"rbuf_size      = %d\n"
				"ext_len      = %d\n"
				"bl_pwm_en      = %d\n"
				"apl	      = 0x%x\n"
				"en_on          = %d\n"
				"en_off         = %d\n"
				"dim_max        = 0x%03x\n"
				"dim_min        = 0x%03x\n"
				"test_flag        = %d\n",
				dev_drv->index,
				rgb->dev_on_flag,
				rgb->vsync_cnt,
				dev_drv->spi_conf.spi_dev_num,
				rgb->spi,
				rgb->tbuf_size,
				rgb->rbuf_size,
				rgb->ext_len,
				rgb->bl_pwm_en,
				rgb->apl,
				dev_drv->en_gpio_on,
				dev_drv->en_gpio_off,
				dev_drv->dim_max,
				dev_drv->dim_min,
				rgb->test_flag);
	} else {
		return sprintf(buf, "invalid node\n");
	}

	return len;
}

static ssize_t rgb_store(const struct class *class, const struct class_attribute *attr,
			    const char *buf, size_t count)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	unsigned int val, val2, val3;
	int n = 0;
	char *buf_orig, *ps, *token;
	char **parm = NULL;
	char str[3] = {' ', '\n', '\0'};

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return count;
	parm = kcalloc(128, sizeof(char *), GFP_KERNEL);
	if (!parm)
		goto rgb_store_end;

	ps = buf_orig;
	while (1) {
		token = strsep(&ps, str);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if (!strcmp(parm[0], "init")) {
		mutex_lock(&dev_mutex);
		rgb_hw_init_on(dev_drv);
		mutex_unlock(&dev_mutex);
	} else if (!strcmp(parm[0], "test_flag")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto rgb_store_err;
			rgb->test_flag = (unsigned char)val;
		}
		LDIMPR("test_flag: %d\n", rgb->test_flag);
	} else if (!strcmp(parm[0], "duty_r")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto rgb_store_err;
			rgb_conf.test_duty[0] = (unsigned short)val;
		}
		LDIMPR("rgb.duty_r: %d\n", rgb_conf.test_duty[0]);
	} else if (!strcmp(parm[0], "duty_g")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto rgb_store_err;
			rgb_conf.test_duty[1] = (unsigned short)val;
		}
		LDIMPR("rgb.duty_g: %d\n", rgb_conf.test_duty[1]);
	} else if (!strcmp(parm[0], "duty_b")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto rgb_store_err;
			rgb_conf.test_duty[2] = (unsigned short)val;
		}
		LDIMPR("rgb.duty_b: %d\n", rgb_conf.test_duty[2]);
	} else if (!strcmp(parm[0], "duty")) {
		if (parm[3]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto rgb_store_err;
			if (kstrtouint(parm[2], 0, &val2) < 0)
				goto rgb_store_err;
			if (kstrtouint(parm[3], 0, &val3) < 0)
				goto rgb_store_err;
			rgb_conf.test_duty[0] = (unsigned short)val;
			rgb_conf.test_duty[1] = (unsigned short)val2;
			rgb_conf.test_duty[2] = (unsigned short)val3;
		}
		LDIMPR("rgb.duty: %d : %d : %d\n",
		rgb_conf.test_duty[0], rgb_conf.test_duty[1], rgb_conf.test_duty[2]);
	} else if (!strcmp(parm[0], "ave")) {
		if (parm[3]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto rgb_store_err;
			if (kstrtouint(parm[2], 0, &val2) < 0)
				goto rgb_store_err;
			if (kstrtouint(parm[3], 0, &val3) < 0)
				goto rgb_store_err;
			rgb_conf.ave[0] = (unsigned short)val;
			rgb_conf.ave[1] = (unsigned short)val2;
			rgb_conf.ave[2] = (unsigned short)val3;
		}
		LDIMPR("rgb.ave: %d : %d : %d\n",
		rgb_conf.ave[0], rgb_conf.ave[1], rgb_conf.ave[2]);
	} else if (!strcmp(parm[0], "max")) {
		if (parm[3]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto rgb_store_err;
			if (kstrtouint(parm[2], 0, &val2) < 0)
				goto rgb_store_err;
			if (kstrtouint(parm[3], 0, &val3) < 0)
				goto rgb_store_err;
			rgb_conf.max[0] = (unsigned short)val;
			rgb_conf.max[1] = (unsigned short)val2;
			rgb_conf.max[2] = (unsigned short)val3;
		}
		LDIMPR("rgb.max: %d : %d : %d\n",
		rgb_conf.max[0], rgb_conf.max[1], rgb_conf.max[2]);
	} else if (!strcmp(parm[0], "min")) {
		if (parm[3]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto rgb_store_err;
			if (kstrtouint(parm[2], 0, &val2) < 0)
				goto rgb_store_err;
			if (kstrtouint(parm[3], 0, &val3) < 0)
				goto rgb_store_err;
			rgb_conf.min[0] = (unsigned short)val;
			rgb_conf.min[1] = (unsigned short)val2;
			rgb_conf.min[2] = (unsigned short)val3;
		}
		LDIMPR("rgb.min: %d : %d : %d\n",
		rgb_conf.min[0], rgb_conf.min[1], rgb_conf.min[2]);
	} else if (!strcmp(parm[0], "iset")) {
		if (parm[3]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto rgb_store_err;
			if (kstrtouint(parm[2], 0, &val2) < 0)
				goto rgb_store_err;
			if (kstrtouint(parm[3], 0, &val3) < 0)
				goto rgb_store_err;
			rgb_conf.iset[0] = (unsigned short)val;
			rgb_conf.iset[1] = (unsigned short)val2;
			rgb_conf.iset[2] = (unsigned short)val3;
		}
		LDIMPR("rgb.iset: %d : %d : %d\n",
		rgb_conf.iset[0], rgb_conf.iset[1], rgb_conf.iset[2]);
	} else if (!strcmp(parm[0], "checksum")) {
		if (parm[2]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto rgb_store_err;
			if (kstrtouint(parm[2], 0, &val2) < 0)
				goto rgb_store_err;

			rgb_conf.checksum[0] = (unsigned short)val;
			rgb_conf.checksum[1] = (unsigned short)val2;
		}
		LDIMPR("rgb.checksum: 0x%x : 0x%x\n",
		rgb_conf.checksum[0], rgb_conf.checksum[1]);
	} else {
		goto rgb_store_err;
	}

rgb_store_end:
	kfree(buf_orig);
	kfree(parm);
	return count;

rgb_store_err:
	pr_info("invalid cmd!!!\n");
	kfree(buf_orig);
	kfree(parm);
	return -EINVAL;
}

static struct class_attribute rgb_class_attrs[] = {
	__ATTR(init, 0644, NULL, rgb_store),
	__ATTR(rgb_status, 0644, rgb_show, NULL),
};

static int rgb_ldim_dev_update(struct ldim_dev_driver_s *dev_drv)
{
	dev_drv->power_on = rgb_power_on;
	dev_drv->power_off = rgb_power_off;
	dev_drv->dev_transmit = rgb_transmit;
	dev_drv->dev_transmit_dummy = rgb_transmit_dummy;
	dev_drv->dev_fb_handle = rgb_fb_handle;
	dev_drv->config_update = rgb_config_update;

	dev_drv->reg_write = NULL;
	dev_drv->reg_read = NULL;
	return 0;
}

int ldim_dev_rgb_probe(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	struct spicc_controller_data *cdata;
	struct spi_private_data *priv;
	struct data_pkt_s data_pkt;
	int i, j;

	if (!dev_drv) {
		LDIMERR("dev_drv is null\n");
		return -1;
	}
	if (!dev_drv->spi_conf.spi_dev[0]) {
		LDIMERR("spi_dev[0] is null\n");
		return -1;
	}

	rgb = kzalloc(sizeof(*rgb), GFP_KERNEL);
	if (!rgb)
		return -1;

	rgb->dev_on_flag = 0;
	rgb->vsync_cnt = 0;
	rgb->zone_num = dev_drv->bl_row * dev_drv->bl_col;
	LDIMPR("rgb->zone_num=%d\n", rgb->zone_num);

	data_pkt = dev_drv->spi_conf.data_pkt;
	rgb->pkt_type = data_pkt.pkt_type;

	rgb->header_cnt = data_pkt.header_cnt;
	rgb->header_data = data_pkt.header_data;
	rgb->extend_cnt = data_pkt.extend_cnt;
	rgb->extend_data = data_pkt.extend_data;
	rgb->adim_width = data_pkt.adim_width;
	rgb->pdim_width = data_pkt.pdim_width;
	rgb->adim = data_pkt.adim;
	rgb->pdim = data_pkt.pdim;

	/* header + data + suffix */
	/* according custom backlight mcu spi spec to set tbuf_size */
	if (rgb->pkt_type == 0) {//640 rgb 2lane
		rgb->ext_len = 30;//(14 + 1) * 2
		rgb->tbuf_size = rgb->zone_num * 3;//half size

		/*packet length*/
		rgb->tbuf_size += rgb->ext_len;
		rgb->rbuf_size = rgb->tbuf_size;
	} else if (rgb->pkt_type == 1) {//512 rgb 1lane
		rgb->ext_len = 5;//(4 + 1)
		rgb->tbuf_size = rgb->zone_num * 6;//6byte per zone

		/*packet length*/
		rgb->tbuf_size += rgb->ext_len;
		rgb->rbuf_size = rgb->tbuf_size;
	} else if (rgb->pkt_type == 2) {//26*15=390 rgb, 3lane
		rgb->ext_len = 6;//(4 + 2)
		rgb->tbuf_size = rgb->zone_num * 4;//4byte 16bit iset + 16 pwm

		/*packet length*/
		rgb->tbuf_size += rgb->ext_len;
		rgb->rbuf_size = rgb->tbuf_size;
	} else if (rgb->pkt_type == 3) {
		//0byte header,0byte suffix,16bit duty(6bit current + 10bit duty)
		rgb->ext_len = 0;
		rgb->tbuf_size = rgb->zone_num * 6;

		/*packet length*/
		rgb->rbuf_size = rgb->tbuf_size;
	}
	LDIMPR("ext_len=%d, tbuf_size=%d\n", rgb->ext_len, rgb->tbuf_size);

	/* for 2nd spi */
	/* here For reference only, need to be set according to the actual device spec!!!*/
	for (j = 0; j < dev_drv->spi_conf.spi_dev_num; j++) {
		rgb->spi[j] = dev_drv->spi_conf.spi_dev[j];
		rgb->rbuf[j] = kcalloc(rgb->rbuf_size,
			sizeof(unsigned char), GFP_KERNEL | GFP_DMA);
		if (!rgb->rbuf[j]) {
			LDIMERR("rgb->rbuf[%d] is error\n", j);
			goto ldim_dev_rgb_probe_err2;
		}

		rgb->tbuf[j] = kcalloc(rgb->tbuf_size,
			sizeof(unsigned char), GFP_KERNEL | GFP_DMA);
		if (!rgb->tbuf[j]) {
			LDIMERR("rgb->tbuf[%d] is error\n", j);
			goto ldim_dev_rgb_probe_err2;
		}

		cdata = rgb->spi[j]->controller_data;
		if (!cdata) {
			LDIMERR("controller_data[%d] is null\n", j);
			goto ldim_dev_rgb_probe_err2;
		}

		priv = cdata->priv;
		if (priv) {
			priv->xlen = rgb->tbuf_size;
		} else {
			LDIMERR("priv[%d] is null\n", j);
			goto ldim_dev_rgb_probe_err2;
		}
	}

	rgb_ldim_dev_update(dev_drv);

	if (dev_drv->class) {
		for (i = 0; i < ARRAY_SIZE(rgb_class_attrs); i++) {
			if (class_create_file(dev_drv->class, &rgb_class_attrs[i])) {
				LDIMERR("create ldim_dev class attribute %s fail\n",
					rgb_class_attrs[i].attr.name);
			}
		}
	}

	rgb->dev_on_flag = 1; /* default enable in uboot */

	LDIMPR("rgb probe ok\n");
	return 0;

ldim_dev_rgb_probe_err2:
	for (j = 0; j < dev_drv->spi_conf.spi_dev_num; j++) {
		kfree(rgb->tbuf[j]);
		kfree(rgb->rbuf[j]);
	}
	rgb->tbuf_size = 0;
	rgb->rbuf_size = 0;
	kfree(rgb);
	rgb = NULL;
	return -1;
}

int ldim_dev_rgb_remove(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	int j;

	if (!rgb)
		return 0;

	for (j = 0; j < dev_drv->spi_conf.spi_dev_num; j++) {
		kfree(rgb->tbuf[j]);
		kfree(rgb->rbuf[j]);
	}
	rgb->tbuf_size = 0;
	rgb->rbuf_size = 0;
	kfree(rgb);
	rgb = NULL;

	return 0;
}
