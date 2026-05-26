/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __INC_AML_BL_H
#define __INC_AML_BL_H
#include <linux/workqueue.h>
#include <linux/cdev.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pwm.h>
#include <linux/amlogic/pwm-meson.h>
#include <linux/amlogic/media/vout/lcd/aml_lcd.h>

#define BLPR(fmt, args...)      pr_info("bl: " fmt "", ## args)
#define BLERR(fmt, args...)     pr_err("bl error: " fmt "", ## args)
#define BLWARN(fmt, args...)     pr_warn("bl warning: " fmt "", ## args)
#define AML_BL_NAME		"aml-bl"

#define BL_LEVEL_FULL_SCALE    4095
#define BL_PWM_DUTY_FULL_SCALE 4095

#define PWM_MAP_RESCALING      0
#define PWM_MAP_NORMALIZATION  1

#define XTAL_FREQ_HZ		(24 * 1000 * 1000) /* unit: Hz */
#define XTAL_HALF_FREQ_HZ	(24 * 1000 * 500)  /* 24M/2 in HZ */

#define BL_FREQ_DEFAULT		1000 /* unit: HZ */
#define BL_FREQ_VS_DEFAULT	2    /* multiple 2 of vfreq */

//#define BL_BRIGHTNESS_METER

struct bl_data_s {
	unsigned int chip_type;
	const char *chip_name;
	unsigned int pwm_vs_flag;
	unsigned int pwm_vs_shift;
};

/* for lcd backlight power */
enum bl_ctrl_method_e {
	BL_CTRL_GPIO = 0,
	BL_CTRL_PWM,
	BL_CTRL_PWM_COMBO,
	BL_CTRL_PWM_ARRAY,
	BL_CTRL_LOCAL_DIMMING,
	BL_CTRL_EXTERN,
	BL_CTRL_MAX,
};

enum bl_pwm_method_e {
	BL_PWM_NEGATIVE = 0,
	BL_PWM_POSITIVE,
	BL_PWM_METHOD_MAX,
};

enum bl_pwm_port_e {
	BL_PWM_A = 0,
	BL_PWM_B,
	BL_PWM_C,
	BL_PWM_D,
	BL_PWM_E,
	BL_PWM_F,
	BL_PWM_G,
	BL_PWM_H,
	BL_PWM_I,
	BL_PWM_J,
	BL_PWM_K,
	BL_PWM_L,
	BL_PWM_M,
	BL_PWM_N,
	BL_PWM_AO_A = 0x50,
	BL_PWM_AO_B,
	BL_PWM_AO_C,
	BL_PWM_AO_D,
	BL_PWM_AO_E,
	BL_PWM_AO_F,
	BL_PWM_AO_G,
	BL_PWM_AO_H,
	BL_PWM_VS = 0xa0,
	BL_PWM_MAX = 0xff,
};

enum bl_off_policy_e {
	BL_OFF_POLICY_NONE = 0,
	BL_OFF_POLICY_ALWAYS,
	BL_OFF_POLICY_ONCE,
	BL_OFF_POLICY_MAX,
};

#define BL_LEVEL_MASK                        0xfff
#define BL_POLICY_POWER_ON_BIT               12
#define BL_POLICY_POWER_ON_MASK              3

#define BL_GPIO_MAX             0xff
#define BL_GPIO_NUM_MAX         5
struct bl_gpio_s {
	char name[LCD_CPU_GPIO_NAME_MAX];
	struct gpio_desc *gpio;
	int probe_flag;
	int register_flag;
};

struct pwm_data_s {
	unsigned int meson_index;
	struct pwm_state state;
	struct pwm_device *pwm;
	struct meson_pwm *meson;
};

struct pwm_curve_s {
	unsigned int curve_duty[5]; //out
	int curve_level[5]; //in
	unsigned int turn_point_duty; //out
	int turn_point_level; //in
	int step_level;
	unsigned int en;
};

struct bl_pwm_config_s {
	unsigned int index;
	unsigned int drv_index;
	struct pwm_data_s pwm_data;
	enum bl_pwm_method_e pwm_method;
	enum bl_pwm_port_e pwm_port;
	int level_max; /* 12bit */
	int level_min; /* 12bit */

	unsigned int pwm_freq; /* pwm_vs: 1~4(vfreq), pwm: freq(unit: Hz) */
	unsigned int pwm_duty; /* 12bit */
	unsigned int pwm_duty_dft; /* 12bit */
	unsigned int pwm_duty_save; /* 12bit, for power on recovery */
	unsigned int pwm_duty_max; /* 12bit */
	unsigned int pwm_duty_min; /* 12bit */
	unsigned int pwm_phase;	/*pwm_vs phase base on vsync*/

	unsigned int pwm_cnt; /* internal used for pwm control */
	unsigned int pwm_pre_div; /* internal used for pwm control */
	unsigned int pwm_value; /* internal used for pwm control */

	struct pwm_curve_s pwm_curve;
};

#define BL_NAME_MAX    30
struct bl_config_s {
	unsigned int index;
	char name[BL_NAME_MAX];
	char pinmux_name[BL_NAME_MAX];
	int level_min;
	int level_max;
	unsigned int ldim_flag;

	int level_in_scale;
	unsigned int pwm_duty_in_scale;
	unsigned int pwm_mapping_method;

	enum bl_ctrl_method_e method;
	unsigned int en_gpio;
	unsigned int en_gpio_on;
	unsigned int en_gpio_off;
	unsigned short power_on_delay;
	unsigned short power_off_delay;
	unsigned int en_sequence_reverse;

	struct bl_pwm_config_s *bl_pwm;
	struct bl_pwm_config_s *bl_pwm_array[4];
	struct bl_pwm_config_s *bl_pwm_combo0;
	struct bl_pwm_config_s *bl_pwm_combo1;
	struct bl_pwm_config_s *bl_pwm_switch;
	struct bl_pwm_config_s *bl_pwm_default;
	enum bl_pwm_port_e bl_pwm_switch_port;
	unsigned int bl_pwm_switch_freq;
	unsigned int bl_pwm_switch_flag;
	unsigned int pwm_on_delay;
	unsigned int pwm_off_delay;
	unsigned short bl_hold_on;

	struct bl_gpio_s bl_gpio[BL_GPIO_NUM_MAX];
	unsigned int bl_extern_index;
};

#ifdef BL_BRIGHTNESS_METER
#define BL_LEVEL_CNT_MAX	      3600 //default 1h
struct bl_metrics_config_s {
	unsigned int frame_rate;
	unsigned int level_count;
	unsigned int brightness_count;
	unsigned int level_metrics;
	unsigned int brightness_metrics;
	unsigned int cnt;
	unsigned int sum_cnt;
	unsigned int times;
	unsigned int *level_buf;
	unsigned int *brightness_buf;
};
#endif

/* backlight_properties: state */
/* Flags used to signal drivers of state changes */
/* Upper 4 bits in bl props are reserved for driver internal use */
#define BL_STATE_DEBUG_FORCE_EN       BIT(8)
#define BL_STATE_PWM_SWITCH           BIT(7)
#define BL_STATE_PRE_ON               BIT(6)
#define BL_STATE_GD_EN                BIT(4)
#define BL_STATE_LCD_ON               BIT(3)
#define BL_STATE_BL_INIT_ON           BIT(2)
#define BL_STATE_BL_POWER_ON          BIT(1)
#define BL_STATE_BL_ON                BIT(0)

struct aml_bl_drv_s {
	unsigned int index;
	unsigned int key_valid;
	unsigned int retry_cnt;
	unsigned char config_load;
	unsigned int state;
	int level;
	int level_brightness;
	int level_gd;

	unsigned char probe_done;
	unsigned char step_on_flag;
	unsigned char on_request; /* for lcd power sequence */
	unsigned char off_policy_cnt; /* bl_off_policy support */
	unsigned char pwm_bypass; /*debug flag*/
	unsigned char pwm_duty_free; /*debug flag*/
	unsigned char debug_force;

#ifdef BL_BRIGHTNESS_METER
	struct bl_metrics_config_s bl_metrics_conf;
#endif
	struct bl_config_s        bconf;
	struct cdev               cdev;
	struct bl_data_s          *data;
	struct platform_device    *pdev;
	struct device             *dev;
	struct backlight_device   *bldev;
	struct delayed_work       config_probe_dly_work;
	struct delayed_work       delayed_on_work;
	int res_ldim_vsync_irq;
	int res_ldim_pwm_vs_irq;
	int res_vsync_irq[3];
	/*struct resource *res_ldim_rdma_irq;*/
	spinlock_t isr_lock;

	struct pinctrl *pin;
	unsigned int pinmux_flag;

	unsigned long long pre_on_time;
};

struct bl_pwm_init_config_s {
	unsigned int *pwm_vs_reg;
	unsigned int pwm_vs_reg_cnt;
	unsigned int pwm_vs_flag;
	unsigned int pwm_vs_shift;
};

struct aml_bl_drv_s *aml_bl_get_driver(int index);
int aml_bl_index_add(int drv_index, int conf_index);

int aml_bl_set_level_brightness(struct aml_bl_drv_s *bdrv, int brightness);
int aml_bl_get_level_brightness(struct aml_bl_drv_s *bdrv);

void bl_pwm_config_init(struct bl_pwm_config_s *bl_pwm);
void bl_pwm_ctrl(struct bl_pwm_config_s *bl_pwm, int status);
struct bl_pwm_init_config_s *get_pwm_init_cfg(void);
void bl_pwm_port_switch(unsigned int state);

#define BL_GPIO_OUTPUT_LOW		0
#define BL_GPIO_OUTPUT_HIGH		1
#define BL_GPIO_INPUT			2

#endif

