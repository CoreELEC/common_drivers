/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _INC_AML_LDIM_H_
#define _INC_AML_LDIM_H_
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/media/vout/lcd/aml_lcd.h>
#include <linux/amlogic/media/vout/lcd/ldim_fw.h>
#include <linux/spi/spi.h>

/*#define LDIM_DEBUG_INFO*/

#define LD_DATA_DEPTH   12
#define LD_DATA_MIN     10
#define LD_DATA_MAX     0xfff

enum ldim_dev_type_e {
	LDIM_DEV_TYPE_NORMAL = 0,
	LDIM_DEV_TYPE_SPI,
	LDIM_DEV_TYPE_I2C,
	LDIM_DEV_TYPE_ABCON,
	LDIM_DEV_TYPE_MAX,
};

struct aml_ldim_driver_s;

struct ldim_config_s {
	unsigned short hsize;
	unsigned short vsize;
	unsigned char seg_row;
	unsigned char seg_col;
	unsigned char bl_mode;
	unsigned char func_en;
	unsigned char dev_index;
};

struct spi_private_data {
	int dev_idx;
	unsigned int xlen;/*actually xfer len*/
	dma_addr_t tx_dma;
	dma_addr_t rx_dma;
	unsigned char *tx_buf;
	unsigned char *rx_buf;
	int async_busy;
	int async_busy_cnt;
	int trig_init;
	int trig_data_ready;
};

struct abcon_conf_s {
	unsigned int tx_clk;
	unsigned int rx_clk;
	unsigned char dev_type;
	unsigned int chip_num[12];
	unsigned char ch_num;
	unsigned char dimming_mode;
	unsigned int gpio_o[3];
	unsigned int gpio_i[3];
	unsigned char idle_level;
	unsigned char fb_en;
	unsigned int fb_det_int;
	unsigned char fb_adj_th;
	unsigned char fb_pwm_dir;
	unsigned char fb_pwm_step;
	unsigned int ctrl;
};

struct data_pkt_s {
	unsigned char pkt_type;
	unsigned char header_cnt;
	unsigned char *header_data;
	unsigned char extend_cnt;
	unsigned char *extend_data;
	unsigned char datamapping_en;
	unsigned char adim_width;
	unsigned char pdim_width;
	unsigned short adim;
	unsigned short pdim;
};

struct spi_conf_s {
	unsigned char trig_mode;
	//unsigned int spi_line_n;/*vpp line n irq*/
	unsigned char rx_en;
	int spi_dev_num;
	int use_ctrl_cs;
	int cs2clk_delay;
	int clk2cs_delay;
	int vs2cs_delay;
	struct spi_device *spi_dev[3];
	struct spi_board_info spi_info[3];
	struct data_pkt_s data_pkt;
};

#define LDIM_DEV_NAME_MAX    30
#define LDIM_INIT_ON_MAX     2000
#define LDIM_INIT_OFF_MAX    100
struct ldim_dev_driver_s {
	unsigned char index;
	char name[LDIM_DEV_NAME_MAX];
	char pinmux_name[LDIM_DEV_NAME_MAX];
	unsigned char key_valid;
	unsigned char config_load;
	unsigned char type;
	unsigned int pwm_phase;
	int en_gpio;
	int en_gpio_on;
	int en_gpio_off;
	int lamp_err_gpio;
	unsigned char fault_check;
	unsigned char write_check;
	unsigned char pinmux_flag;
	unsigned char brightness_mode;
	unsigned int chip_cnt;

	unsigned int bl_row;
	unsigned int bl_col;
	unsigned int dim_min;
	unsigned int dim_max;
	unsigned int dim_adim;
	unsigned int dim_pdim;

	unsigned int zone_num;
	char bl_mapping_path[256];
	unsigned short *bl_mapping;
	char profile_path[256];

	unsigned char init_loaded;
	unsigned char cmd_size;
	unsigned char *init_on;
	unsigned char *init_off;
	unsigned int init_on_cnt;
	unsigned int init_off_cnt;
	unsigned int hw_on_delay;
	unsigned int hw_off_delay;
	unsigned int *boundary_x;
	unsigned int *boundary_y;

	struct bl_pwm_config_s ldim_pwm_config;
	struct bl_pwm_config_s analog_pwm_config;
	struct ldim_boost_s boost_conf;

	struct platform_device *pdev;
	struct pinctrl *pin;
	struct device *dev;
	struct class *class;
	//struct spi_device *spi_dev[2];
	//struct spi_board_info spi_info[2];
	struct abcon_conf_s abcon_conf;
	struct spi_conf_s spi_conf;

	void (*dim_range_update)(struct ldim_dev_driver_s *dev_drv);
	int (*pinmux_ctrl)(struct ldim_dev_driver_s *dev_drv, int status);
	int (*reg_write)(struct ldim_dev_driver_s *dev_drv, unsigned char chip_id,
			 unsigned char reg, unsigned char *buf, unsigned int len);
	int (*reg_read)(struct ldim_dev_driver_s *dev_drv, unsigned char chip_id,
			unsigned char reg, unsigned char *buf, unsigned int len);

	/*device api*/
	int (*power_on)(struct aml_ldim_driver_s *ldim_drv);
	int (*power_off)(struct aml_ldim_driver_s *ldim_drv);
	int (*dev_transmit)(struct aml_ldim_driver_s *ldim_drv,
		       unsigned int *buf, unsigned int len);
	int (*dev_transmit_dummy)(struct aml_ldim_driver_s *ldim_drv);
	int (*dev_fb_handle)(struct aml_ldim_driver_s *ldim_drv);
	int (*pwm_vs_update)(struct aml_ldim_driver_s *ldim_drv);
	void (*config_print)(struct aml_ldim_driver_s *ldim_drv);
	int (*config_update)(struct aml_ldim_driver_s *ldim_drv);
	int (*dev_transmit_start)(void);
	int (*dev_transmit_stop)(void);
};

struct ldim_drv_data_s {
	unsigned char ldc_chip_type;
	unsigned int rsv_mem_size;
	unsigned short h_zone_max;
	unsigned short v_zone_max;
	unsigned short total_zone_max;

	void (*vs_arithmetic)(struct aml_ldim_driver_s *ldim_drv);
	int (*memory_init)(struct platform_device *pdev,
			   struct ldim_drv_data_s *data,
			   unsigned int row, unsigned int col);
	void (*drv_init)(struct aml_ldim_driver_s *ldim_drv);
	void (*func_ctrl)(struct aml_ldim_driver_s *ldim_drv, int flag);
};

struct ldim_spi_trig_param_s {
	unsigned int spi_tbuf_size;
	unsigned int spi_tx_clk;
	unsigned int spi_tx_line_cost;
	unsigned int spi_trig_line;
	unsigned int cs2clk_delay_ns;
	unsigned int ldc_trig_line;
	unsigned int line_time_ns;
	unsigned int line_total_cnt;
	unsigned int lost_frame_cnt;
	unsigned int ldc_trig_line_margin;
};

/*******global API******/
#define LDIM_STATE_POWER_ON             BIT(0)
#define LDIM_STATE_FUNC_EN              BIT(1)
#define LDIM_STATE_REMAP_EN             BIT(2)
#define LDIM_STATE_REMAP_FORCE_UPDATE   BIT(3)
#define LDIM_STATE_LD_EN                BIT(4)
#define LDIM_STATE_TRANSMIT_EN           BIT(5)
#define LDIM_STATE_PQ_INIT              BIT(6)

struct aml_ldim_driver_s {
	unsigned char valid_flag;
	unsigned char static_pic_flag;
	unsigned char vsync_change_flag;
	unsigned char duty_update_flag;
	unsigned char in_vsync_flag;
	unsigned char transmit_div_en;
	unsigned char transmit_div;

	unsigned char init_on_flag;
	unsigned char func_en;
	unsigned char level_idx;
	unsigned char demo_mode;
	unsigned char black_frm_en;
	unsigned char ld_sel;  /* for gd bypass */
	unsigned char func_bypass;  /* for lcd bist pattern */
	unsigned char dev_transmit_bypass;
	unsigned char brightness_bypass;
	unsigned char test_bl_en;
	unsigned char test_bl_cnt;
	unsigned char load_db_en;
	unsigned char level_update;
	unsigned char resolution_update;
	unsigned int debug_ctrl;/*for debug used*/

	unsigned int state;
	unsigned int data_min;
	unsigned int data_max;
	unsigned int brightness_level;
	unsigned int litgain;
	unsigned int dbg_vs_cnt;
	unsigned int irq_cnt;
	unsigned int pwm_vs_irq_cnt;
	unsigned int pwm_vs_irq_err_cnt;
	unsigned long long pwm_vs_irq_time_pre;
	unsigned long long arithmetic_time[10];
	unsigned long long xfer_time[10];
	unsigned long long fw_time[10];
	unsigned char time_msr_en;
	unsigned int level_curve[5][2];

	char *pqdata;
	int ldim_vsync_irq;
	int ldim_pwm_vs_irq;
	int ldim_hist_irq;
	int ldim_line_n_irq;
	struct ldim_drv_data_s *data;
	struct ldim_config_s *conf;
	struct ldim_dev_driver_s *dev_drv;
	struct ldim_spi_trig_param_s trig_param;

	struct ldim_fw_s *fw;
	struct ldim_fw_custom_s *cus_fw;

	unsigned int *test_matrix;
	unsigned int *local_bl_matrix;
	unsigned int *bl_matrix_cur;
	unsigned int *bl_matrix_pre;

	/* driver api */
	int (*init)(void);
	int (*power_on)(void);
	int (*power_off)(void);
	int (*set_level)(unsigned int level);
	void (*test_ctrl)(int flag);
	void (*ld_sel_ctrl)(int flag);
	void (*pwm_vs_update)(void);
	void (*config_print)(void);
};

struct aml_ldim_driver_s *aml_ldim_get_driver(void);

char aml_ldim_get_bbd_state(void);
int aml_ldim_get_config_dts(struct device_node *child);
int aml_ldim_get_config_unifykey(unsigned char *buf);
int aml_ldim_get_config_json(int panel_id);
int aml_ldim_get_config_ini(void *inip, void *psec);
int aml_ldim_probe(struct platform_device *pdev);
int aml_ldim_remove(void);
void ldim_trig_line_update(struct aml_ldim_driver_s *ldim_drv);

#endif

