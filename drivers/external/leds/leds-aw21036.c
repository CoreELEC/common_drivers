/*
* leds-aw21036.c   aw21036 led module
*
* Version: 1.0.0
*
* Copyright (c) 2017 AWINIC Technology CO., LTD
*
*  Author: Nick Li <liweilei@awinic.com.cn>
*
* This program is free software; you can redistribute  it and/or modify it
* under  the terms of  the GNU General  Public License as published by the
* Free Software Foundation;  either version 2 of the  License, or (at your
* option) any later version.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/leds.h>

#include "leds-aw21036.h"

#define AW21036_I2C_NAME	"aw21036_led"
#define AW21036_VERSION "v2.0.0"

#define AW_I2C_RETRIES              5
#define AW_I2C_RETRY_DELAY          5
#define AW_READ_CHIPID_RETRIES      5
#define AW_READ_CHIPID_RETRY_DELAY  5
#define AW21036_COLORS_COUNT        3
#define AW21036_MAX_IO              36  //LED 24

#define REG_GCR                     0x00

//*******************************************************BR
//LED(R_G_B)    LED1-LED5, nums:5*3=15
#define REG_BR0                     0x01     //1+0

//LED(R_G_B)    LED6-LED8, nums:3*3=9
#define REG_BR24                    0x19    //1+24

//LED(R_G_B)    LED9, nums:1*3=3
#define REG_BR33                    0x22    //1+33

//LED(R_G_B)    RCU - RJ45 - WIFI, 3*(1*3)= 9
#define REG_BR15                    0x10
#define REG_BR18                    0x13
#define REG_BR21                    0x16

#define REG_UPDATE                  0x49    //Write 0x00 to update BR

//*******************************************************COL
//LED(R_G_B)    LED1-LED5,          nums:5*3=15
#define REG_COL0                    0x4A     //(74+0)
//LED(R_G_B)    LED6-LED8,          nums:3*3=9
#define REG_COL24                   0x62     //(74+24)

//LED(R_G_B)    LED9,               nums:1*3=3
#define REG_COL33                   0x6B     //(74+33)

//LED(R_G_B)    RCU - RJ45 - WIFI, 3*(1*3)= 9
#define REG_COL15                   89      //(74+15)
#define REG_COL18                   92      //(74+18)
#define REG_COL21                   95      //(74+21)

#define REG_OUTPUT_P1               0x6E    //Glogbal current control
#define REG_GCC                     0x6E    //Glogbal current control

//*******************************************************

#define REG_ID                      0x7F
#define REG_SWRST                   0x7F

/* aw21036 register read/write access*/
#define REG_NONE_ACCESS             0
#define REG_RD_ACCESS               1 << 0
#define REG_WR_ACCESS               1 << 1
#define AW21036_REG_MAX             0xFF

#define BR_MAX                      36      //aw21036 max io is 36
#define COL_MAX                     36      //aw21036 max io is 36


#define AW21036_DEBUG_ENABLE

#ifdef AW21036_DEBUG_ENABLE
// #define AW21036_FUNC_DEBUG(fmt, args...)    printk(KERN_INFO "[leds_aw21036]%s: (%d) "fmt"\n", __func__, __LINE__, ##args)
// #define AW21036_FUNC_ENTER(fmt, args...)    printk(KERN_INFO "[leds_aw21036]%s: Enter(%d) "fmt"\n", __func__, __LINE__, ##args)
// #define AW21036_FUNC_EXIT(fmt, args...)     printk(KERN_INFO "[leds_aw21036]%s: Exit(%d) "fmt"\n", __func__, __LINE__, ##args)
#define AW21036_FUNC_DEBUG(fmt, ...)     pr_info("%s() -- %d " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define AW21036_FUNC_ENTER(fmt, ...)     pr_info("%s() -- Enter(%d) " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define AW21036_FUNC_EXIT(fmt, ...)      pr_info("%s() -- Exit(%d) " fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define AW21036_FUNC_DEBUG(fmt, ...)
#define AW21036_FUNC_ENTER(fmt, ...)
#define AW21036_FUNC_EXIT(fmt, ...)
#endif

const unsigned char aw21036_reg_access[AW21036_REG_MAX] = {
[REG_GCR   ]     = REG_RD_ACCESS|REG_WR_ACCESS,
[REG_ID    ]     = REG_RD_ACCESS|REG_WR_ACCESS,
[REG_SWRST ]     = REG_WR_ACCESS|REG_WR_ACCESS,
[REG_BR0   ]     = REG_RD_ACCESS|REG_WR_ACCESS,
[REG_BR24  ]     = REG_RD_ACCESS|REG_WR_ACCESS,
[REG_BR33  ]     = REG_RD_ACCESS|REG_WR_ACCESS,
[REG_BR15  ]     = REG_RD_ACCESS|REG_WR_ACCESS,
[REG_BR18  ]     = REG_RD_ACCESS|REG_WR_ACCESS,
[REG_BR21  ]     = REG_RD_ACCESS|REG_WR_ACCESS,
[REG_UPDATE    ]     = REG_RD_ACCESS|REG_WR_ACCESS,
};

static int aw21036_i2c_writes(struct i2c_client *client, unsigned char sreg_addr, uint8_t length, uint8_t *bufs)
{
	int ret = -1;

	struct i2c_msg msg;
	int i, retries = 0;
	uint8_t data[64];

	data[0]     = sreg_addr;                /*start register addr<--write*/
	for(i = 1; i <= length; i++)
	{
		data[i] = bufs[i-1];
		//AW21036_FUNC_DEBUG("data[%d] = 0x%x\n", i, bufs[i-1]);
	}

	msg.addr      = client->addr;           /*i2c addr*/
	msg.flags     = !I2C_M_RD;              /*write flag  = 0*/
	msg.len       = length + 1;             /*write len bytes*/
	msg.buf       = data;                   /*write data[1] -->data[0] */

	while(retries < 3)
	{
		ret = i2c_transfer(client->adapter, &msg, 1);
		if(ret >= 0)
		{
			//AW21036_FUNC_DEBUG("success....ret = %d\n",ret);
			ret = 1;
			break;
		} else {
			AW21036_FUNC_DEBUG("continue...\n");
		}

		retries++;
	}

	if(retries >= 3)
	{
		AW21036_FUNC_DEBUG("error...retyies = %d\n", retries);
	}

	return ret;
}

static void leds_aw21036b_work_func(struct work_struct *work)
{
	int i, j;

	while(0 == cycles_val) {
		for(j = 0; j < instruct_val; j++)
		{
			//aw21036_i2c_writes(aw21036->i2c, REG_BR0, 15, tmp_w[j] + 0);
			//aw21036_i2c_writes(aw21036->i2c, REG_BR24, 9, tmp_w[j] + 24);
			aw21036_i2c_writes(aw21036->i2c, REG_BR0, BR_MAX, tmp_w[j] + 0); //LED0-LED12 * 3 = 36

			aw21036_i2c_write(aw21036, REG_UPDATE, 0x00);

			usleep_range(1000 * (time_val-10), 1000 * (time_val + 10));

			if(0==state_mode)
				break;
		}

		if(0==state_mode)
			break;
	}

	for(i = 1; i <= cycles_val; i++)
	{
		for(j = 0; j < instruct_val; j++)
		{
			//aw21036_i2c_writes(aw21036->i2c, REG_BR0, 15, &tmp_w[j][0]);
			//aw21036_i2c_writes(aw21036->i2c, REG_BR24, 9, &tmp_w[j][24]);
			aw21036_i2c_writes(aw21036->i2c, REG_BR0, BR_MAX, &tmp_w[j][0]); //LED0-LED12 * 3 = 36

			aw21036_i2c_write(aw21036, REG_UPDATE, 0x00);

			usleep_range(1000 * (time_val-10), 1000 * (time_val + 10));

			if(0==state_mode)
				break;
		}

		if(0==state_mode)
			break;
	}

	AW21036_FUNC_DEBUG("platform_id = %d, driver_init_flag = %d\n", platform_id, driver_init_flag);
	if (platform_id == 15 && driver_init_flag == 0) { //15 is adt-5
		driver_init_flag = 1;
		tmp_color[4] = 0x007f00;
		AW21036_FUNC_DEBUG("set 0x007f00 for power led\n");
		meson_aw21036_set_colors(tmp_color, aw21036->led_counts);
	}

	return;
}

static void init_leds_mode(int leds_mode)
{
	int offset, i, j;
	struct meson_aw21036_io *io;
	time_val         = *(all_data[leds_mode] + 0);
	cycles_val       = *((all_data[leds_mode]) + 1);
	instruct_val     = (length_data[leds_mode] - 2) / 36;

	state_mode = 0;
	usleep_range(80000,100000);

	cancel_work_sync(&aw21036->leds_work);
	flush_workqueue(aw21036->leds_wq);

	if(aw21036->g_time_value > 0) {
		time_val = aw21036->g_time_value;
		AW21036_FUNC_DEBUG("set time_value=%d\n", time_val);
	}

	AW21036_FUNC_DEBUG("leds_mode=%d < time_val=%d, cycles_val=%d, instruct_val=%d >\n",leds_mode,time_val,cycles_val,instruct_val);

	for(i=0; i < instruct_val; i++)                    //instruct_val, value
	{
		for(j = 0; j < 12; j++) //max led_counts is 12, 12x3 = 36 max io
		{
			offset = 36 * i + j * 3;
			if (j >= aw21036->led_counts) {
				AW21036_FUNC_DEBUG("led: %d >= led_counts: %d, skip copy data\n", j, aw21036->led_counts);
				continue;
			}

			io = &aw21036->io[j];
			tmp_w[i][io->r_io] = *((all_data[leds_mode]) + offset + 2);
			tmp_w[i][io->g_io] = *((all_data[leds_mode]) + offset + 2 + 1);
			tmp_w[i][io->b_io] = *((all_data[leds_mode]) + offset + 2 + 2);

			AW21036_FUNC_DEBUG("tmp_w[%d][%d]=%x\n", i, io->r_io, tmp_w[i][io->r_io]);
			//AW21036_FUNC_DEBUG("tmp_w[%d][%d]=%x\n", i, io->g_io, tmp_w[i][io->g_io]);
			//AW21036_FUNC_DEBUG("tmp_w[%d][%d]=%x\n", i, io->b_io, tmp_w[i][io->b_io]);
			//AW21036_FUNC_DEBUG("all_data[%d][%d]=%x\n", leds_mode, offset + 2, all_data[leds_mode][offset + 2]);
			//AW21036_FUNC_DEBUG("all_data[%d][%d]=%x\n", leds_mode, offset + 2 + 1, all_data[leds_mode][offset + 2 + 1]);
			//AW21036_FUNC_DEBUG("all_data[%d][%d]=%x\n", leds_mode, offset + 2 + 2, all_data[leds_mode][offset + 2 + 2]);

			tmp_color[j] = tmp_w[i][io->r_io] << 16 | tmp_w[i][io->g_io] << 8 | tmp_w[i][io->b_io];
		}
	}

	state_mode = leds_mode;
	queue_work(aw21036->leds_wq, &aw21036->leds_work);

	AW21036_FUNC_DEBUG("enter,Line:%d...state_mode=%d\n", __LINE__,state_mode);
}

static void meson_aw21036_set_colors(unsigned int *colors, unsigned int counts)
{
	struct meson_aw21036_colors *color;
	struct meson_aw21036_io *io;
	unsigned char color_data[AW21036_MAX_IO];
	unsigned int i;

	memset(color_data, 0, sizeof(color_data));
	for (i = 0; i < counts; i++) {
		io = &aw21036->io[i];
		if (i >= aw21036->led_counts) {
			AW21036_FUNC_DEBUG("led: %d >= led_counts: %d, skip copy data\n",i, aw21036->led_counts);
			continue;
		}

		color = &aw21036->colors[i];
		color_data[io->b_io] = color->blue = colors[i] & 0xff;
		color_data[io->g_io] = color->green = (colors[i] >> 8) & 0xff;
		color_data[io->r_io] = color->red = (colors[i] >> 16) & 0xff;
	}

	// define index4 is power led, set tmp_color[4]]
	if ((15 == platform_id) && (4 == i)) {
		AW21036_FUNC_DEBUG("tmp_color[4] = 0x%x \n",tmp_color[4]);
		io = &aw21036->io[4];
		color_data[io->b_io] = color->blue = tmp_color[i] & 0xff;
		color_data[io->g_io] = color->green = (tmp_color[i] >> 8) & 0xff;
		color_data[io->r_io] = color->red = (tmp_color[i] >> 16) & 0xff;
	}

	AW21036_FUNC_DEBUG("colors0 = 0x%x \n",colors[0]);
	//aw21036_i2c_writes(aw21036->i2c, REG_DIM00, AW21036_MAX_IO, color_data);
	//aw21036_i2c_writes(aw21036->i2c, REG_BR0, 15, &color_data[0]);	//LED-1~5:LED1~LED15
	//aw21036_i2c_writes(aw21036->i2c, REG_BR24, 9, &color_data[24]);   //LED-6~8:LED25~LED32
	aw21036_i2c_writes(aw21036->i2c, REG_BR0, 36, &color_data[0]);      //LED-1~12:LED1~LED12 * 3 = 36

	aw21036_i2c_write(aw21036, REG_UPDATE, 0x00);
}

static void init_leds_config(void)
{
	aw21036_hw_reset(aw21036);
	//aw21036_i2c_write(aw21036, REG_SWRST, 0x00);      //0x7F  SW_RSTN

	aw21036_i2c_write(aw21036, REG_GCR, 0x01);          //GCR: chip enable

	aw21036_i2c_write(aw21036, 0xAB, 0x00);             //GCFG0: GEn = 0
	aw21036_i2c_write(aw21036, 0xAC, 0x08);             //GCFG1: GCOLDIS = 1
	//aw21036_i2c_write(aw21036, 0x7A, 0x00);           //GCR2: RGBMD = 0,disable
	//aw21036_i2c_write(aw21036, 0xA0, 0x08);           //PATCFG : bit3 : SWITCH <0: LED off, 1: LED on>

	aw21036_i2c_write(aw21036, REG_GCC, 0xFF);          //GCCR: 8bit

	aw21036_i2c_writes(aw21036->i2c, REG_COL0, COL_MAX, tmp_col);
}

static void init_leds_data(void)
{
	all_data[0] = (int *)&data_mode0;
	all_data[1] = (int *)&data_mode1;
	all_data[2] = (int *)&data_mode2;
	all_data[3] = (int *)&data_mode3;
	all_data[4] = (int *)&data_mode4;
	all_data[5] = (int *)&data_mode5;
	all_data[6] = (int *)&data_mode6;
	all_data[7] = (int *)&data_mode7;

	all_data[8] = (int *)&data_mode8;
	all_data[9] = (int *)&data_mode9;
	all_data[10] = (int *)&data_mode10;
	all_data[11] = (int *)&data_mode11;

	/*add X3 SEI600TID */
	all_data[12] = (int *)&data_mode12;

	length_data[0] = sizeof(data_mode0)/sizeof(data_mode0[0]);
	length_data[1] = sizeof(data_mode1)/sizeof(data_mode1[0]);
	length_data[2] = sizeof(data_mode2)/sizeof(data_mode2[0]);
	length_data[3] = sizeof(data_mode3)/sizeof(data_mode3[0]);
	length_data[4] = sizeof(data_mode4)/sizeof(data_mode4[0]);
	length_data[5] = sizeof(data_mode5)/sizeof(data_mode5[0]);
	length_data[6] = sizeof(data_mode6)/sizeof(data_mode6[0]);
	length_data[7] = sizeof(data_mode7)/sizeof(data_mode7[0]);
	length_data[8] = sizeof(data_mode8)/sizeof(data_mode8[0]);
	length_data[9] = sizeof(data_mode9)/sizeof(data_mode9[0]);
	length_data[10] = sizeof(data_mode10)/sizeof(data_mode10[0]);
	length_data[11] = sizeof(data_mode11)/sizeof(data_mode11[0]);

	/*add X3 SEI600TID */
	length_data[12] = sizeof(data_mode12)/sizeof(data_mode12[0]);       //add X3 SEI600TID

	init_leds_config();

	if(platform_id == 3) {
		init_leds_mode(12);            //for SEI600TID
	} else if(platform_id == 2) {
		init_leds_mode(1);             //for SEI520
	} else if(platform_id == 4) {
		init_leds_mode(1);             //for SEI610F
	} else {
		init_leds_mode(6);
	}

}

static int aw21036_i2c_write(struct meson_aw21036 *aw21036, unsigned char reg_addr, unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while(cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_write_byte_data(aw21036->i2c, reg_addr, reg_data);
		if(ret < 0) {
			AW21036_FUNC_DEBUG("i2c_write cnt=%d error=%d\n", cnt, ret);
		} else {
			break;
		}
		cnt ++;
		msleep(AW_I2C_RETRY_DELAY);
	}

	return ret;
}

static int aw21036_i2c_read(struct meson_aw21036 *aw21036, unsigned char reg_addr, unsigned int *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while(cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data(aw21036->i2c, reg_addr);
		if(ret < 0) {
			AW21036_FUNC_DEBUG("i2c_read cnt=%d error=%d\n", cnt, ret);
		} else {
			*reg_data = ret;
			break;
		}
		cnt ++;
		msleep(AW_I2C_RETRY_DELAY);
	}

	return ret;
}

static void aw21036_set_brightness(struct led_classdev *cdev, enum led_brightness brightness)
{
	struct meson_aw21036 *aw21036 = container_of(cdev, struct meson_aw21036, cdev);

	aw21036->cdev.brightness = brightness;

	//schedule_work(&aw21036->brightness_work);
}

static int aw21036_parse_dt(struct device *dev, struct meson_aw21036 *aw21036, struct device_node *np)
{
	//int ret;

	aw21036->reset_gpiod = devm_gpiod_get_optional(dev, "reset", GPIOD_ASIS);
	if (IS_ERR(aw21036->reset_gpiod)) {
		dev_err(dev, "%s: no reset gpio provided, will not HW reset device\n", __func__);
		aw21036->reset_gpiod = NULL;
	} else if (aw21036->reset_gpiod) {
		dev_info(dev, "%s: reset gpio provided ok\n", __func__);
	}

	//ret = of_property_read_u32(np, "p1_all_led", &(aw21036->p1_all_led));
	//if (ret  < 0) {
	//	dev_err(dev, "%s: no p1_all_led provided\n", __func__);
	//	return -1;
	//} else {
	//	printk("aw21036->p1_all_led = %d\n",aw21036->p1_all_led);
	//	dev_info(dev, "%s: p1_all_led provided ok\n", __func__);
	//}

	return 0;
}

static int aw21036_hw_reset(struct meson_aw21036 *aw21036)
{
	AW21036_FUNC_ENTER("enter\n");

    if (aw21036 && aw21036->reset_gpiod) {
		// use internal soft reset
		aw21036_i2c_write(aw21036, REG_SWRST, 0x00);   //soft reset
		usleep_range(10000,15000);

		// set CHIPEN to 1
		gpiod_set_value_cansleep(aw21036->reset_gpiod, 0);
		msleep(2);
		gpiod_set_value_cansleep(aw21036->reset_gpiod, 1);
		msleep(2);
	} else {
		// use internal soft reset
		aw21036_i2c_write(aw21036, REG_SWRST, 0x00);   //0x7F  SW_RSTN
		msleep(2);
		dev_err(aw21036->dev, "%s: failed, switch to SW_RSTN!\n", __func__);
	}

	AW21036_FUNC_EXIT("exit\n");
	return 0;
}

static int aw21036_hw_off(struct meson_aw21036 *aw21036)
{
	AW21036_FUNC_ENTER("enter\n");

	if (aw21036 && aw21036->reset_gpiod) {
		gpiod_set_value_cansleep(aw21036->reset_gpiod, 0);
		msleep(1);
	} else {
		//aw21036_i2c_write(aw21036, REG_SWRST, 0x00);   //0x7F  SW_RSTN
		dev_err(aw21036->dev, "%s:  failed\n", __func__);
	}

	AW21036_FUNC_EXIT("exit\n");
	return 0;
}

static int aw21036_read_chipid(struct meson_aw21036 *aw21036)
{
	int ret = -1;
	unsigned char cnt = 0;
	unsigned int reg_val = 0;
	AW21036_FUNC_ENTER("enter\n");

// [SEI-Mack.Zhang 2025-12-02 ]user hw reset function {
	if (aw21036->reset_gpiod) {
		// use internal soft reset
		aw21036_i2c_write(aw21036, REG_SWRST, 0x00);   //soft reset
		usleep_range(10000,15000);

		//gpiod_set_pull(aw21036->reset_gpiod, GPIOD_PULL_UP);
		gpiod_direction_output(aw21036->reset_gpiod, 0);
		//gpiod_set_value_cansleep(aw21036->reset_gpiod, 0);
		msleep(1);
		gpiod_set_value_cansleep(aw21036->reset_gpiod, 1);
		msleep(1);

		pr_info("aw21036]%s use hw reset:%d\n", __func__, __LINE__);
	} else { //use sw reset function
		aw21036_i2c_write(aw21036, REG_SWRST, 0x00);   //soft reset
		usleep_range(80000,100000);

		pr_info("aw21036]%s use sw reset:%d\n", __func__,__LINE__);
	}
	// [SEI-Mack.Zhang 2025-12-02 ]user hw reset function }

	while(cnt < AW_READ_CHIPID_RETRIES) {
		ret = aw21036_i2c_read(aw21036, REG_ID, &reg_val);
		if (ret < 0) {
			AW21036_FUNC_DEBUG("failed to read register aw21036_REG_ID: %d\n", ret);
			return -EIO;
		}
		switch (reg_val) {
			case AW21036_ID:
				AW21036_FUNC_DEBUG("aw21036 detected\n");
				aw21036->chipid = AW21036_ID;
				return 0;
			default:
				break;
		}

		cnt ++;
		msleep(AW_READ_CHIPID_RETRY_DELAY);
	}

	AW21036_FUNC_EXIT("exit\n");
	return -EINVAL;
}

/******************************************************
*
* sys group attribute: mode
*
******************************************************/
static ssize_t aw21036_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	char *after;
	//struct led_classdev *led_cdev = dev_get_drvdata(dev);
	//struct meson_aw21036 *aw21036 = container_of(led_cdev, struct meson_aw21036, cdev);
	unsigned int state_mode = 0;
	AW21036_FUNC_ENTER("enter\n");

	//AW21036_FUNC_DEBUG("state_mode=%d\n", state_mode);
	state_mode = simple_strtoul(buf, &after, 10);           //convert to 10
	AW21036_FUNC_DEBUG("state_mode=%d\n", state_mode);

	if(state_mode < 0 || state_mode > 12) {
		AW21036_FUNC_DEBUG("state_mode is out of range\n");
		return -EINVAL;
	}
	init_leds_mode(state_mode);

	AW21036_FUNC_EXIT("exit\n");
	return count;
}

static ssize_t aw21036_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len, "current mode:%d\n",state_mode);

	return len;
}


/******************************************************
*
* sys group attribute: reg
*
******************************************************/
static ssize_t aw21036_reg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct meson_aw21036 *aw21036 = container_of(led_cdev, struct meson_aw21036, cdev);
	unsigned int databuf[2] = {0, 0};
	AW21036_FUNC_ENTER("enter\n");

	if(2 == sscanf(buf, "%x %x", &databuf[0], &databuf[1])) {
		aw21036_i2c_write(aw21036, (unsigned char)databuf[0], (unsigned char)databuf[1]);
	}

	AW21036_FUNC_EXIT("exit\n");
	return count;
}

static ssize_t aw21036_reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct meson_aw21036 *aw21036 = container_of(led_cdev, struct meson_aw21036, cdev);

	ssize_t len = 0;
	unsigned char i = 0;
	unsigned int reg_val = 0;

	for(i = 0; i < AW21036_REG_MAX; i ++) {
		if(!(aw21036_reg_access[i]&REG_RD_ACCESS))
			continue;

		aw21036_i2c_read(aw21036, i, &reg_val);
		len += snprintf(buf+len, PAGE_SIZE-len, "reg: 0x%02x = 0x%02x\n", i, reg_val);
	}

	return len;
}

/**
* sys group attribute: hwen
* 1: gpio reset
* 2: sw-reset
* 3: init_leds_data
*/
static ssize_t aw21036_hwen_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct meson_aw21036 *aw21036 = container_of(led_cdev, struct meson_aw21036, cdev);
	unsigned int databuf[1] = {0};

	AW21036_FUNC_ENTER("enter\n");

	if(1 == sscanf(buf, "%x", &databuf[0])) {
		if(1 == databuf[0]) {
			aw21036_hw_reset(aw21036);
		}else if(2 == databuf[0]) {                      //sw-reset
			aw21036_i2c_write(aw21036, 0x7F, 0x00);      //0x7F  SW_RSTN
		}else if(3 == databuf[0]) {                      //sw-init
			init_leds_data();
		}else{
			aw21036_hw_off(aw21036);
		}

		hwen_id = databuf[0];
	}

	AW21036_FUNC_EXIT("exit\n");
	return count;
}

static ssize_t aw21036_hwen_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	//struct led_classdev *led_cdev = dev_get_drvdata(dev);
	//struct meson_aw21036 *aw21036 = container_of(led_cdev, struct meson_aw21036, cdev);
	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len, "hwen=%d\n", hwen_id);

	return len;
}

static ssize_t aw21036_colors_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{

	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct meson_aw21036 *aw21036 = container_of(led_cdev, struct meson_aw21036, cdev);
	unsigned int colors[9] = {0};
	int ret = -1, i = 0;
	AW21036_FUNC_ENTER("enter\n");
	//AW21036_FUNC_DEBUG("colors0 = 0x%x...0\n",colors[0]);

	ret = sscanf(buf, "%x %x %x %x", &colors[0], &colors[1], &colors[2], &colors[3]);
	AW21036_FUNC_DEBUG("colors[0] = 0x%x, colors[1] = 0x%x, colors[2] = 0x%x, colors[3] = 0x%x\n", colors[0], colors[1], colors[2], colors[3]);
	AW21036_FUNC_DEBUG("ret: %d\n", ret);
	if (ret != 4)
		AW21036_FUNC_DEBUG("set led colors fail ret: %d\n", ret);
	if (ret == 4) {
		for(i = 0; i < 4; i++) {
			tmp_color[i] = colors[i];
		}

		meson_aw21036_set_colors(tmp_color, 4);
	} else if (ret == 2) {    //for android 11, param: index, color
		if (15 == platform_id) {
			if (colors[0] == 4) {
				AW21036_FUNC_DEBUG("ignore led data\n");
				return count;
			}
		}

		if (colors[0] < aw21036->led_counts) {
			tmp_color[colors[0]] = colors[1];
		}

		meson_aw21036_set_colors(tmp_color, 4);
	}

	AW21036_FUNC_EXIT("exit\n");
	return count;
}

static ssize_t aw21036_colors_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len, "0x%x 0x%x 0x%x 0x%x - 0x%x 0x%x 0x%x 0x%x - 0x%x 0x%x 0x%x 0x%x\n", tmp_color[0], tmp_color[1], tmp_color[2], tmp_color[3], tmp_color[4], tmp_color[5], tmp_color[6], tmp_color[7], tmp_color[8], tmp_color[9], tmp_color[10], tmp_color[11]);

	return len;
}

/**
* sys group attribute: multi_colors
* param: index, color
*/
static ssize_t aw21036_multi_colors_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	//struct led_classdev *led_cdev = dev_get_drvdata(dev);
	//struct meson_aw21036 *aw21036 = container_of(led_cdev, struct meson_aw21036, cdev);
	unsigned int colors[14] = {0};
	int ret = -1, i = 0;
	AW21036_FUNC_ENTER("enter\n");
	//AW21036_FUNC_DEBUG("colors0 = 0x%x...0\n",colors[0]);

	ret = sscanf(buf, "%x %x %x %x %x %x %x %x %x %x %x %x %x", &colors[0], &colors[1], &colors[2], &colors[3], &colors[4], &colors[5], &colors[6], &colors[7], &colors[8], &colors[9], &colors[10], &colors[11], &colors[12]);

	AW21036_FUNC_DEBUG("ret: %d\n", ret);
	if (ret == 13) {
		for(i = 0; i < 12; i++) {   //max led counts is 12
			tmp_color[i] = colors[i + 1];
		}

		meson_aw21036_set_colors(tmp_color, 12);
	} else if (ret == 2) {    //for android 11, param: index, color
		if (colors[0] < aw21036->led_counts)
			tmp_color[colors[0]] = colors[1];

		//meson_aw21036_set_colors(tmp_color, 4);
		meson_aw21036_set_colors(tmp_color, aw21036->led_counts);
	}

	AW21036_FUNC_DEBUG("colors[0]: %x, colors[1]: %x, colors[2]: %x, colors[3]: %x, colors[4]: %x, colors[5]: %x, colors[6]: %x, colors[7]: %x, colors[8]: %x, colors[9]: %x, colors[10]: %x, colors[11]: %x, colors[12]: %x\n",
		colors[0], colors[1], colors[2], colors[3], colors[4], colors[5], colors[6], colors[7], colors[8], colors[9], colors[10], colors[11], colors[12]);

	return count;
}

static ssize_t aw21036_multi_colors_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len, "0x%x 0x%x 0x%x 0x%x - 0x%x 0x%x 0x%x 0x%x - 0x%x 0x%x 0x%x 0x%x\n", tmp_color[0], tmp_color[1], tmp_color[2], tmp_color[3], tmp_color[4], tmp_color[5], tmp_color[6], tmp_color[7], tmp_color[8], tmp_color[9], tmp_color[10], tmp_color[11]);

	return len;
}

static ssize_t aw21036_time_value_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct meson_aw21036 *aw21036 = container_of(led_cdev, struct meson_aw21036, cdev);
	unsigned int databuf[1] = {0};
	AW21036_FUNC_ENTER("enter\n");

	if(!aw21036) {
		AW21036_FUNC_DEBUG("aw21036 is NULL\n");
		return -EINVAL;
	}

	if(1 == sscanf(buf, "%x", &databuf[0])) {
		aw21036->g_time_value = databuf[0];
		AW21036_FUNC_DEBUG("time_value=%x\n", aw21036->g_time_value);
	}

	AW21036_FUNC_EXIT("exit\n");
	return count;
}

static ssize_t aw21036_time_value_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct meson_aw21036 *aw21036 = container_of(led_cdev, struct meson_aw21036, cdev);
	ssize_t len = 0;
	AW21036_FUNC_ENTER("enter\n");

	if(!aw21036) {
		AW21036_FUNC_DEBUG("aw21036 is NULL\n");
		return -EINVAL;
	}

	len += snprintf(buf+len, PAGE_SIZE-len, "time_value:%d\n", aw21036->g_time_value);

	AW21036_FUNC_EXIT("exit\n");
	return len;
}


static ssize_t aw21036_led1_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct meson_aw21036 *aw21036 = container_of(led_cdev, struct meson_aw21036, cdev);
	unsigned int databuf[1] = {0};
	AW21036_FUNC_ENTER("enter\n");

	if(1 == sscanf(buf, "%x", &databuf[0])) {
		if(databuf[0] == 0) {            //off
			led_value[0] = 0;
			led_value[1] = 0;
			led_value[2] = 0;
		} else if(databuf[0] == 1) {        //on
			led_value[0] = 0x68;
			led_value[1] = 0xdd;
			led_value[2] = 0xf5;
		} else {
			led_value[0] = (databuf[0] >> 16) & 0xFF;    //R
			led_value[1] = (databuf[0] >> 8) & 0xFF;     //G
			led_value[2] = databuf[0] & 0xFF;            //B
		}

		//AW21036_FUNC_DEBUG("set mode=%d\n", databuf[0]);
		AW21036_FUNC_DEBUG("set R=%x,G=%x,B=%x\n", led_value[0], led_value[1], led_value[2]);

		aw21036_i2c_writes(aw21036->i2c, REG_BR21, 3, led_value);
		aw21036_i2c_write(aw21036, REG_UPDATE, 0x00);
	}

	AW21036_FUNC_EXIT("exit\n");
	return count;
}

static ssize_t aw21036_led1_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	//struct led_classdev *led_cdev = dev_get_drvdata(dev);
	//struct meson_aw21036 *aw21036 = container_of(led_cdev, struct meson_aw21036, cdev);
	ssize_t len = 0;

	aw21036_i2c_read(aw21036, 0x10, &ledr_value);
	aw21036_i2c_read(aw21036, 0x11, &ledg_value);
	aw21036_i2c_read(aw21036, 0x12, &ledb_value);

	len += snprintf(buf+len, PAGE_SIZE-len, "led1:%d %d %d\n", ledr_value, ledg_value, ledb_value);

	return len;
}

static ssize_t aw21036_led2_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct meson_aw21036 *aw21036 = container_of(led_cdev, struct meson_aw21036, cdev);
	unsigned int databuf[1] = {0};
	AW21036_FUNC_ENTER("enter\n");

	if(1 == sscanf(buf, "%x", &databuf[0])) {
		if(databuf[0] == 0) {        //off
			led_value[0] = 0;
			led_value[1] = 0;
			led_value[2] = 0;
		} else if(databuf[0] == 1) {    //on
			led_value[0] = 0x68;
			led_value[1] = 0xdd;
			led_value[2] = 0xf5;
		} else {
			led_value[0] = (databuf[0] >> 16) & 0xFF;    //R
			led_value[1] = (databuf[0] >> 8) & 0xFF;     //G
			led_value[2] = databuf[0] & 0xFF;            //B
		}

		//AW21036_FUNC_DEBUG("set mode=%d\n", databuf[0]);
		AW21036_FUNC_DEBUG("set R=%x,G=%x,B=%x\n", led_value[0], led_value[1], led_value[2]);

		aw21036_i2c_writes(aw21036->i2c, REG_BR18, 3, led_value);
		aw21036_i2c_write(aw21036, REG_UPDATE, 0x00);
	}

	AW21036_FUNC_EXIT("exit\n");
	return count;
}

static ssize_t aw21036_led2_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	//struct led_classdev *led_cdev = dev_get_drvdata(dev);
	//struct meson_aw21036 *aw21036 = container_of(led_cdev, struct meson_aw21036, cdev);
	ssize_t len = 0;
	AW21036_FUNC_ENTER("enter\n");

	aw21036_i2c_read(aw21036, 0x13, &ledr_value);
	aw21036_i2c_read(aw21036, 0x14, &ledg_value);
	aw21036_i2c_read(aw21036, 0x15, &ledb_value);

	len += snprintf(buf+len, PAGE_SIZE-len, "led2:%d %d %d\n", ledr_value, ledg_value, ledb_value);

	AW21036_FUNC_EXIT("exit\n");
	return len;
}

static ssize_t aw21036_led3_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct meson_aw21036 *aw21036 = container_of(led_cdev, struct meson_aw21036, cdev);
	unsigned int databuf[1] = {0};
	AW21036_FUNC_ENTER("enter\n");

	if(1 == sscanf(buf, "%x", &databuf[0])) {
		if(databuf[0] == 0) {            //off
			led_value[0] = 0;
			led_value[1] = 0;
			led_value[2] = 0;
		} else if(databuf[0] == 1) {        //on
			led_value[0] = 0x68;
			led_value[1] = 0xdd;
			led_value[2] = 0xf5;
		} else {
			led_value[0] = (databuf[0] >> 16) & 0xFF;    //R
			led_value[1] = (databuf[0] >> 8) & 0xFF;     //G
			led_value[2] = databuf[0] & 0xFF;            //B
		}

		//AW21036_FUNC_DEBUG("set mode=%d\n", databuf[0]);
		AW21036_FUNC_DEBUG("set R=%x,G=%x,B=%x\n", led_value[0], led_value[1], led_value[2]);

		aw21036_i2c_writes(aw21036->i2c, REG_BR15, 3, led_value);
		aw21036_i2c_write(aw21036, REG_UPDATE, 0x00);
	}

	AW21036_FUNC_EXIT("exit\n");
	return count;
}

static ssize_t aw21036_led3_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	//struct led_classdev *led_cdev = dev_get_drvdata(dev);
	//struct meson_aw21036 *aw21036 = container_of(led_cdev, struct meson_aw21036, cdev);
	ssize_t len = 0;
	AW21036_FUNC_ENTER("enter\n");

	aw21036_i2c_read(aw21036, 0x16, &ledr_value);
	aw21036_i2c_read(aw21036, 0x17, &ledg_value);
	aw21036_i2c_read(aw21036, 0x18, &ledb_value);

	len += snprintf(buf + len, PAGE_SIZE - len, "led3:%d %d %d\n", ledr_value, ledg_value, ledb_value);

	AW21036_FUNC_EXIT("exit\n");
	return len;
}

static DEVICE_ATTR(reg, S_IWUSR | S_IRUGO, aw21036_reg_show, aw21036_reg_store);
static DEVICE_ATTR(hwen, S_IWUSR | S_IRUGO, aw21036_hwen_show, aw21036_hwen_store);
static DEVICE_ATTR(mode, S_IWUSR | S_IRUGO, aw21036_mode_show, aw21036_mode_store);
static DEVICE_ATTR(single_colors, S_IWUSR | S_IRUGO, aw21036_colors_show, aw21036_colors_store);
static DEVICE_ATTR(multi_colors, S_IWUSR | S_IRUGO, aw21036_multi_colors_show, aw21036_multi_colors_store);
static DEVICE_ATTR(time_value, S_IWUSR | S_IRUGO, aw21036_time_value_show, aw21036_time_value_store);

static DEVICE_ATTR(led1, S_IWUSR | S_IRUGO, aw21036_led1_show, aw21036_led1_store);
static DEVICE_ATTR(led2, S_IWUSR | S_IRUGO, aw21036_led2_show, aw21036_led2_store);
static DEVICE_ATTR(led3, S_IWUSR | S_IRUGO, aw21036_led3_show, aw21036_led3_store);

static struct attribute *aw21036_attributes[] = {
	&dev_attr_reg.attr,
	&dev_attr_hwen.attr,
	&dev_attr_mode.attr,
	&dev_attr_single_colors.attr,
	&dev_attr_multi_colors.attr,
	&dev_attr_time_value.attr,
	&dev_attr_led1.attr,
	&dev_attr_led2.attr,
	&dev_attr_led3.attr,
	NULL
};

static struct attribute_group aw21036_attribute_group = {
	.attrs = aw21036_attributes
};

/*[SEI-Tiger.Hu-2020/05/15]add for aw21036 early suspend & resume { */
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
static void aw21036b_early_suspend(struct early_suspend *h)
{
	//struct meson_aw21036 *ldev = (struct meson_aw21036 *)h->param;
	AW21036_FUNC_ENTER("enter\n");
	AW21036_FUNC_ENTER("enter\n");
	if(3 == platform_id) {
		init_leds_mode(0);        //off
		//aw21036_i2c_write(aw21036, REG_OUTPUT_P1, 0xF0); //OOOO DDDD: 1111 0000
		//aw21036_i2c_writes(aw21036->i2c, REG_BR15, 9, tmp_ledoff);
		aw21036_i2c_write(aw21036, REG_UPDATE, 0x00);
	}else if(2 == platform_id) {
		init_leds_mode(6);        //multi-colored horse
	} else if(15 == platform_id) {
		AW21036_FUNC_DEBUG("set 0x7f0000 for power led\n");
		tmp_color[4] = 0x7f0000;
		meson_aw21036_set_colors(tmp_color, aw21036->led_counts);
	}

	AW21036_FUNC_EXIT("exit\n");
}

static void aw21036b_late_resume(struct early_suspend *h)
{
	//struct meson_aw21036 *ldev = (struct meson_aw21036 *)h->param;
	AW21036_FUNC_ENTER("enter\n");
	AW21036_FUNC_ENTER("enter\n");

	if(3 == platform_id) {
		init_leds_mode(12);        //user-blue
		//aw21036_i2c_write(aw21036, REG_OUTPUT_P1, 0x00); //OOOO DDDD: 0000 0000
		//aw21036_i2c_writes(aw21036->i2c, REG_BR15, 9, tmp_ledon);
		aw21036_i2c_write(aw21036, REG_UPDATE, 0x00);
	}else if(2 == platform_id) {
		init_leds_mode(11);        //white
	} else if(15 == platform_id) {
		AW21036_FUNC_DEBUG("set 0x007f00 for power led\n");
		tmp_color[4] = 0x007f00;
		meson_aw21036_set_colors(tmp_color, aw21036->led_counts);
	}

	AW21036_FUNC_EXIT("exit\n");
}

static struct early_suspend aw21036b_early_suspend_handler;
#endif
/*[SEI-Tiger.Hu-2020/05/15]add for aw21036 early suspend & resume } */

/******************************************************
*
* led class dev
*
******************************************************/
static int aw21036_parse_led_cdev(struct meson_aw21036 *aw21036, struct device_node *np)
{
	struct device_node *temp;
	unsigned int colors[AW21036_COLORS_COUNT];
	int ret = -1;
	int i = 0;
	AW21036_FUNC_ENTER("enter\n");

	for_each_child_of_node(np, temp) {
		ret = of_property_read_u32_array(temp, "default_colors",
						colors, AW21036_COLORS_COUNT);
		if (ret < 0) {
			AW21036_FUNC_DEBUG("Failure reading default colors ret = %d\n", ret);
			goto free_pdata;
		}

		ret = of_property_read_u32(temp, "r_io_number", &(aw21036->io[i].r_io));
		if (ret < 0) {
			AW21036_FUNC_DEBUG("Failure reading r_io_number ret = %d\n", ret);
			goto free_pdata;
		}

		ret = of_property_read_u32(temp, "g_io_number", &(aw21036->io[i].g_io));
		if (ret < 0) {
			AW21036_FUNC_DEBUG("Failure reading g_io_number ret = %d\n", ret);
			goto free_pdata;
		}

		ret = of_property_read_u32(temp, "b_io_number", &(aw21036->io[i].b_io));
		if (ret < 0) {
			AW21036_FUNC_DEBUG("Failure reading b_io_number ret = %d\n", ret);
			goto free_pdata;
		}

		aw21036->colors[i].red = colors[0];
		aw21036->colors[i].green = colors[1];
		aw21036->colors[i].blue = colors[2];

		AW21036_FUNC_DEBUG("led[%d] default_colors=%d %d %d\n", i, colors[0], colors[1], colors[2]);
		AW21036_FUNC_DEBUG("led[%d] r_io_number=%d, g_io_number=%d, b_io_number=%d\n", i, aw21036->io[i].r_io, aw21036->io[i].g_io, aw21036->io[i].b_io);
		i++;
	}

	ret = of_property_read_u32(np, "platform", &platform_id);
	if (ret < 0) {
		AW21036_FUNC_DEBUG("Failure reading platform ret = %d\n", ret);
	}

	AW21036_FUNC_DEBUG("set platform_id = %d\n", platform_id);

	//if(platform_id == 3)
	//	aw21036->cdev.name = "aw9523_led";    //For the same API as the previous aw9523b by SEI600TID
	//else
	aw21036->cdev.name = "i2c_leds";

	aw21036->cdev.brightness = 0;
	aw21036->cdev.max_brightness = 255;
	aw21036->cdev.brightness_set = aw21036_set_brightness;

	ret = led_classdev_register(aw21036->dev, &aw21036->cdev);
	if (ret) {
		AW21036_FUNC_DEBUG("unable to register led ret=%d\n", ret);
		goto free_pdata;
	}

	ret = sysfs_create_group(&aw21036->cdev.dev->kobj, &aw21036_attribute_group);
	if (ret) {
		AW21036_FUNC_DEBUG("create aw21036 sysfs failed, ret: %d\n", ret);
		goto free_class;
	}

	AW21036_FUNC_EXIT("exit\n");
	return 0;

free_class:
	led_classdev_unregister(&aw21036->cdev);
	AW21036_FUNC_EXIT("exit\n");
free_pdata:
	AW21036_FUNC_EXIT("exit\n");
	return ret;
}

/******************************************************
*
* i2c driver
*
******************************************************/
static int aw21036_i2c_probe(struct i2c_client *i2c)
{
//    struct meson_aw21036 *aw21036;
	struct device_node *np = i2c->dev.of_node;
	int ret;

	AW21036_FUNC_ENTER("enter\n");

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C|I2C_FUNC_SMBUS_EMUL)) {
		AW21036_FUNC_DEBUG("check_functionality failed\n");
		AW21036_FUNC_EXIT("exit\n");
		return -EIO;
	}

	aw21036 = devm_kzalloc(&i2c->dev, sizeof(struct meson_aw21036), GFP_KERNEL);
	if (aw21036 == NULL)
		return -ENOMEM;

	aw21036->dev = &i2c->dev;
	aw21036->i2c = i2c;

	aw21036->led_counts = device_get_child_node_count(&i2c->dev);
	aw21036->io = devm_kcalloc(&i2c->dev, aw21036->led_counts, sizeof(struct meson_aw21036_io),
				GFP_KERNEL);
	if (aw21036->io == NULL)
		return -ENOMEM;

	aw21036->colors = devm_kcalloc(&i2c->dev, aw21036->led_counts, sizeof(struct meson_aw21036_colors),
				GFP_KERNEL);
	if (aw21036->colors == NULL)
		return -ENOMEM;

	//i2c_set_clientdata(i2c, aw21036);
	//dev_set_drvdata(&i2c->dev, aw21036);

	i2c_set_clientdata(i2c, aw21036);
	ret = aw21036_parse_dt(aw21036->dev, aw21036, np);
	/* aw21036 chip id */
	ret = aw21036_read_chipid(aw21036);
	if (ret < 0) {
		AW21036_FUNC_DEBUG("aw21036_read_chipid failed ret=%d\n", ret);
		goto err_id;
	}

	dev_set_drvdata(&i2c->dev, aw21036);

	aw21036_parse_led_cdev(aw21036, np);
	if (ret < 0) {
		AW21036_FUNC_DEBUG("error creating led class dev\n");
		goto err_sysfs;
	}

	INIT_WORK(&aw21036->leds_work, leds_aw21036b_work_func);
	aw21036->leds_wq= create_singlethread_workqueue("leds_wq");
	//aw21036->leds_wq= create_workqueue("leds_wq");
	if (aw21036->leds_wq== NULL) {
		AW21036_FUNC_DEBUG("create leds_work->input_wq fail!\n");
		return -ENOMEM;
	}
	//queue_work(aw21036->leds_wq, &aw21036->leds_work);

/*[SEI-Tiger.Hu-2020/05/15]add for aw21036 early suspend&resume { */
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	aw21036b_early_suspend_handler.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 10;
	aw21036b_early_suspend_handler.suspend = aw21036b_early_suspend;
	aw21036b_early_suspend_handler.resume = aw21036b_late_resume;
	aw21036b_early_suspend_handler.param = aw21036;
	register_early_suspend(&aw21036b_early_suspend_handler);
#endif
/*[SEI-Tiger.Hu-2020/05/15]add for aw21036 early suspend&resume } */

	init_leds_data();

	AW21036_FUNC_EXIT("exit\n");
	return 0;
err_sysfs:
	AW21036_FUNC_EXIT("exit\n");
err_id:
	AW21036_FUNC_EXIT("exit\n");
	return ret;
}

static void aw21036_i2c_remove(struct i2c_client *i2c)
{
	struct meson_aw21036 *aw21036 = i2c_get_clientdata(i2c);

	AW21036_FUNC_ENTER("enter\n");
	if(!aw21036) {
		AW21036_FUNC_DEBUG("aw21036 is NULL\n");
		AW21036_FUNC_EXIT("exit\n");
		return;
	}

	aw21036_hw_reset(aw21036);

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	unregister_early_suspend(&aw21036b_early_suspend_handler);
	AW21036_FUNC_DEBUG("early suspend unregistered\n");
#endif

	cancel_work_sync(&aw21036->leds_work);
	AW21036_FUNC_DEBUG("cancel_work_sync\n");
	destroy_workqueue(aw21036->leds_wq);
	AW21036_FUNC_DEBUG("destroy_workqueue\n");

	sysfs_remove_group(&aw21036->cdev.dev->kobj, &aw21036_attribute_group);
	AW21036_FUNC_DEBUG("sysfs attributes removed\n");
	led_classdev_unregister(&aw21036->cdev);
	AW21036_FUNC_DEBUG("led_classdev_unregister\n");

    /* reset_gpiod is managed by devm, released automatically */

	devm_kfree(&i2c->dev, aw21036);
	AW21036_FUNC_DEBUG("devm_kfree\n");

	AW21036_FUNC_EXIT("exit\n");

	return;
}

static void aw21036_i2c_shutdown(struct i2c_client *i2c)
{
	struct meson_aw21036 *aw21036 = i2c_get_clientdata(i2c);

	AW21036_FUNC_ENTER("enter\n");
	if(!aw21036) {
		AW21036_FUNC_DEBUG("aw21036 is NULL\n");
		AW21036_FUNC_EXIT("exit\n");
		return;
	}

	if(15 == platform_id) {
		AW21036_FUNC_DEBUG("set 0x000000 for power led\n");
		tmp_color[4] = 0x000000;
		meson_aw21036_set_colors(tmp_color, aw21036->led_counts);
	}

	AW21036_FUNC_EXIT("exit\n");
}

static const struct i2c_device_id aw21036_i2c_id[] = {
	{ AW21036_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, aw21036_i2c_id);

static struct of_device_id aw21036_dt_match[] = {
	{ .compatible = "seirobotics,aw21036_led" },
	{ },
};

static struct i2c_driver meson_aw21036_driver = {
	.driver = {
		.name = AW21036_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(aw21036_dt_match),
	},
	.probe = aw21036_i2c_probe,
	.remove = aw21036_i2c_remove,
	.shutdown = aw21036_i2c_shutdown,
	.id_table = aw21036_i2c_id,
};

int led_aw21036_sei_init(void)
{
	AW21036_FUNC_ENTER("enter\n");
	return i2c_add_driver(&meson_aw21036_driver);
}

void __exit led_aw21036_sei_exit(void)
{
	AW21036_FUNC_ENTER("enter\n");
	return i2c_del_driver(&meson_aw21036_driver);
}

module_init(led_aw21036_sei_init);
module_exit(led_aw21036_sei_exit);

MODULE_AUTHOR("Tiger Hu <huyh@seirobotics.net>");
MODULE_DESCRIPTION("AW21036 LED Driver");
MODULE_LICENSE("GPL v2");
