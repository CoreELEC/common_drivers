/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_LCD_TCON_H__
#define __AML_LCD_TCON_H__
#include <linux/dma-map-ops.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_tcon_data.h>
#include <linux/amlogic/media/vout/lcd/lcd_tcon_fw.h>

#define AML_TCON_CLASS_NAME  "aml_tcon"
#define AML_TCON_DEVICE_NAME "tcon"

#define REG_LCD_TCON_MAX    0xffff
#define TCON_INTR_MASKN_VAL    0x0  /* default mask all */

//#define TCON_DBG_TIME

struct lcd_tcon_init_setting_s {
	unsigned int reg;
	unsigned int val;
	unsigned int bit;
	unsigned int len;
};

struct lcd_tcon_axi_mem_cfg_s {
	unsigned int mem_type;
	unsigned int mem_size;
	unsigned int axi_reg;  //ddrif reg
	unsigned int mem_valid;
};

struct lcd_tcon_dma_ops_s {
	int status;
	struct  list_head *addr_list;
	int (*get_frame_cnt)(struct aml_lcd_drv_s *pdrv);
	void (*start)(struct aml_lcd_drv_s *pdrv);
	void (*stop)(struct aml_lcd_drv_s *pdrv);
	void (*mif_set)(struct aml_lcd_drv_s *pdrv, phys_addr_t paddr, unsigned int size);
	void (*init)(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_dma_ops_s *ops);
	void (*deinit)(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_dma_ops_s *ops);
	void (*update)(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_dma_ops_s *ops);
	int (*is_done)(struct aml_lcd_drv_s *pdrv);
	void (*clr_done)(struct aml_lcd_drv_s *pdrv);
	int (*wait_done)(struct aml_lcd_drv_s *pdrv, unsigned int timeout_us);

	void (*sw_mif_set)(struct aml_lcd_drv_s *pdrv, phys_addr_t paddr, unsigned int size);
	void (*sw_trigger)(struct aml_lcd_drv_s *pdrv);
	int (*sw_is_done)(struct aml_lcd_drv_s *pdrv);
	int (*sw_wait_done)(struct aml_lcd_drv_s *pdrv, unsigned int timeout_us);
	void (*sw_clr_done)(struct aml_lcd_drv_s *pdrv);
};

struct lcd_tcon_reg_skip_s {
	unsigned int start_reg;
	unsigned int end_reg;
};

#define TCON_BIN_VER_LEN    9
struct tcon_core_reg_info_s {
	char bin_ver[TCON_BIN_VER_LEN];
	struct lcd_tcon_init_block_header_s *header;
	struct lcd_tcon_init_block_ext_header_s ext_header;
	unsigned int table_size;
	unsigned char *table;
	char *user_info;
};

struct lcd_tcon_config_s {
	unsigned char tcon_valid;
	unsigned int chip_type;
	unsigned int core_reg_ver;
	unsigned int core_reg_width;
	unsigned int reg_table_width;
	unsigned int reg_table_len;
	unsigned int core_reg_start;
	unsigned int top_reg_base;
	struct lcd_tcon_reg_skip_s *reg_skip_tbl;  //skip flush core reg range
	unsigned int reg_skip_tbl_len;

	unsigned int axi_bank;

	unsigned int rsv_mem_size;
	unsigned int axi_mem_size;
	unsigned int bin_path_size;
	unsigned int secure_cfg_size;
	unsigned int vfp_tail;  //tcon vfp lines requirement

	struct lcd_tcon_init_setting_s *init_pre_setting;
	struct lcd_tcon_init_setting_s *init_post_setting;
	unsigned int axi_tbl_len;
	struct lcd_tcon_axi_mem_cfg_s *axi_mem_cfg_tbl;
	struct lcd_tcon_dma_ops_s *lut_dma_ops;

	void (*tcon_axi_mem_config)(void);
	void (*tcon_axi_mem_secure)(void);
	void (*tcon_init_table_pre_proc)(struct tcon_core_reg_info_s *core_reg_info);
	void (*tcon_init_table_post_proc)(struct aml_lcd_drv_s *pdrv);
	void (*tcon_lut_post_proc)(struct aml_lcd_drv_s *pdrv, unsigned int lut_type);
	void (*tcon_global_reset)(struct aml_lcd_drv_s *pdrv);
	int (*tcon_top_init)(struct aml_lcd_drv_s *pdrv);
	int (*tcon_enable)(struct aml_lcd_drv_s *pdrv);
	int (*tcon_disable)(struct aml_lcd_drv_s *pdrv);
	int (*tcon_lock_get_status)(struct aml_lcd_drv_s *pdrv);
	int (*tcon_check)(struct aml_lcd_drv_s *pdrv, struct lcd_detail_timing_s *ptiming,
			struct tcon_core_reg_info_s *core_reg_info, char *ferr_str, char *warn_str);

};

struct tcon_rmem_config_s {
	phys_addr_t mem_paddr;
	unsigned char *mem_vaddr;
	unsigned int mem_size;
};

struct tcon_sec_mem_config_s {
	phys_addr_t mem_paddr;
	unsigned char *mem_vaddr;
	unsigned int mem_size;
	unsigned int sec_handle;
	unsigned int sec_protect;
};

struct tcon_rmem_s {
	unsigned char flag;
	int use_lrm;
	unsigned int rsv_mem_size;
	unsigned int axi_mem_size;
	unsigned int axi_bank;

	void *rsv_mem_vaddr;
	phys_addr_t rsv_mem_paddr;
	phys_addr_t axi_mem_paddr;
	phys_addr_t sw_mem_paddr;

	unsigned int *axi_reg;
	struct tcon_rmem_config_s *axi_rmem;
	struct tcon_rmem_config_s bin_path_rmem;
	struct tcon_rmem_config_s secure_cfg_rmem;
	struct tcon_sec_mem_config_s secure_axi_rmem;
};

struct tcon_multi_range_s {
	unsigned int max;
	unsigned int min;
};

struct tcon_multi_resolution_s {
	unsigned int hsize;
	unsigned int vsize;
};

struct tcon_multi_val_s {
	struct tcon_multi_range_s range;
	struct tcon_multi_resolution_s resolution;
};

struct tcon_data_list_s {
	unsigned int id;
	unsigned int ctrl_method;
	struct tcon_multi_val_s multi;
	unsigned int ctrl_data_cnt;
	unsigned int *ctrl_data;
	char *block_name;
	unsigned char *block_vaddr;
	phys_addr_t block_paddr;
	struct tcon_data_list_s *next;
};

struct tcon_data_multi_s {
	unsigned int id;
	unsigned int block_type;
	unsigned int list_cnt;
	unsigned int bypass_flag;
	unsigned int switch_flag;
	struct tcon_data_list_s *list_header;
	struct tcon_data_list_s *list_cur;
	struct tcon_data_list_s *list_remove;
	char dbg_str[64];
};

struct tcon_data_init_s {
	unsigned int block_type;
	unsigned int priority;
	unsigned int flag;
	struct tcon_data_list_s *list_header;
};

struct tcon_mem_map_table_s {
	/*header*/
	unsigned int version;
	unsigned int data_load_level;
	unsigned int block_cnt;
	//unsigned char init_load;
	unsigned char data_complete;
	unsigned char bin_path_valid;
	unsigned char effect_id;
	unsigned char effect_type;

	unsigned int lut_valid_flag;

	unsigned int *data_size;
	unsigned char **data_mem_vaddr;
	dma_addr_t *data_mem_paddr;

	unsigned int multi_lut_update;
	unsigned int data_multi_cnt;
	struct tcon_data_multi_s *data_multi;

	struct tcon_data_init_s *data_init;

#ifdef TCON_DBG_TIME
	unsigned long long vsync_time[10];
#endif
};

#define FR_DETECT_LEVEL_MAX 20
struct lcd_tcon_vrr_data_s {
	unsigned short disp_h_active;
	unsigned short disp_v_active;
	unsigned short disp_frame_rate_min;
	unsigned short disp_frame_rate_max;
	unsigned int fr_level[FR_DETECT_LEVEL_MAX];
	unsigned short fr_count;
	phys_addr_t paddr;
	unsigned int size;
	unsigned int part_size;
	unsigned int part;
	unsigned int en;
	unsigned int support;
	unsigned int ready;
};

#define IP27_TRIG_APTU 1
#define IP27_TRIG_SELF 0
struct lcd_tcon_ip27_s {
	unsigned char trig_mode;
	unsigned char en;
	unsigned char valid;
	unsigned char cfg_valid;
	unsigned char step;
	unsigned char init;
	unsigned char init_en;
	unsigned char vcom_order;
	unsigned char data_mode;
	unsigned char debug;
	unsigned char i2c_ready;
	unsigned char i2c_addr;
	unsigned char i2c_bytes;
	unsigned char i2c_reg_offset;
	unsigned int i2c_speed;
	unsigned int trig_line;//for mode 1 aptu intr trig
	unsigned int trig_line_dbg;
};

#define MEM_FLAG_MAX	    2
struct lcd_tcon_local_cfg_s {
	spinlock_t multi_list_lock; /* for tcon multi lut list change */
	struct tcon_core_reg_info_s def_core_info;
	struct tcon_core_reg_info_s cur_core_info;

	struct list_head pdf_data_list;  //for struct lcd_tcon_pdf_data_s
	unsigned char pdf_list_load_flag;
	struct lcd_tcon_vrr_data_s vrr_data;
	struct lcd_tcon_ip27_s ip27;

	struct cdev   cdev;
	struct device *dev;
	dev_t devno;
	struct class *clsp;
};

#if IS_ENABLED(CONFIG_AMLOGIC_TEE)
void lcd_tcon_mem_tee_get_status(void);
int lcd_tcon_mem_tee_protect(int protect_en);
#endif

struct lcd_tcon_config_s *get_lcd_tcon_config(void);
struct tcon_rmem_s *get_lcd_tcon_rmem(void);
struct tcon_mem_map_table_s *get_lcd_tcon_mm_table(void);
struct lcd_tcon_local_cfg_s *get_lcd_tcon_local_cfg(void);
void lcd_tcon_fw_lut_update_status(struct aml_lcd_drv_s *pdrv);
void lcd_tcon_fw_prepare(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_config_s *tcon_conf);
void lcd_tcon_fw_finish(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_fw_buf_table_generate(struct lcd_tcon_fw_s *tcon_fw);
void lcd_tcon_fw_base_timing_update(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_fw_add_core_table(struct lcd_tcon_fw_s *fw, unsigned char *core_table);
void lcd_tcon_fw_remove_core_table(struct lcd_tcon_fw_s *fw);
int lcd_tcon_fw_core_table_cnt(struct lcd_tcon_fw_s *fw);
void lcd_tcon_fw_update_core(struct lcd_tcon_fw_s *fw);

/* **********************************
 * tcon config
 * **********************************
 */
/* common */
#define TCON_VAC_SET_PARAM_NUM		 3
#define TCON_VAC_LUT_PARAM_NUM		 256

/* T5 */
#define LCD_TCON_CORE_REG_WIDTH_T5       32
#define LCD_TCON_TABLE_WIDTH_T5          32
#define LCD_TCON_TABLE_LEN_T5            0x18d4 /* 0x635*4 */
#define LCD_TCON_AXI_BANK_T5             2

#define TCON_CORE_REG_START_T5           0x0100

/* T5D */
#define LCD_TCON_CORE_REG_WIDTH_T5D       32
#define LCD_TCON_TABLE_WIDTH_T5D          32
#define LCD_TCON_TABLE_LEN_T5D            0x102c /* 0x40b*4 */
#define LCD_TCON_AXI_BANK_T5D             1

#define TCON_CORE_REG_START_T5D           0x0100

/* T3X */
#define LCD_TCON_TABLE_LEN_T3X            0x1478 /* 0x51e*4 */
#define LCD_TCON_AXI_BANK_T3X             3

/* TXHD2 */
#define LCD_TCON_TABLE_LEN_TXHD2          0x1670 /* 0x59c*4 */
#define LCD_TCON_AXI_BANK_TXHD2           2

/* T6D */
#define LCD_TCON_TABLE_LEN_T6D            0x1668 /* 0x59a*4 */
#define LCD_TCON_AXI_BANK_T6D             2

/* T6W */
#define TCON_CORE_REG_START_T6W           0x0
#define LCD_TCON_TABLE_LEN_T6W            0x57ac /* 0x15eb*4 */
#define LCD_TCON_AXI_BANK_T6W             2

/* **********************************
 * tcon api
 * **********************************
 */
/* internal */
int tcon_get_lock_status_t5m(struct aml_lcd_drv_s *pdrv);
int tcon_get_lock_status_t6w(struct aml_lcd_drv_s *pdrv);
int tcon_lut_dma_get_frame_cnt(struct aml_lcd_drv_s *pdrv);
int tcon_lut_dma_get_frame_cnt_t6w(struct aml_lcd_drv_s *pdrv);
void tcon_lut_dma_start(struct aml_lcd_drv_s *pdrv);
void tcon_lut_dma_stop(struct aml_lcd_drv_s *pdrv);
void tcon_lut_dma_start_t6d(struct aml_lcd_drv_s *pdrv);
void tcon_lut_dma_stop_t6d(struct aml_lcd_drv_s *pdrv);
void tcon_lut_dma_mif_set(struct aml_lcd_drv_s *pdrv, phys_addr_t paddr, unsigned int size);
void tcon_lut_dma_mif_set_t6w(struct aml_lcd_drv_s *pdrv, phys_addr_t paddr, unsigned int size);
void tcon_lut_dma_init_t5m(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_dma_ops_s *ops);
void tcon_lut_dma_init_t3x(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_dma_ops_s *ops);
void tcon_lut_dma_init_t6w(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_dma_ops_s *ops);
void tcon_lut_dma_deinit(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_dma_ops_s *ops);
void lcd_tcon_lut_dma_update(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_dma_ops_s *ops);
void tcon_vrr_dma_mif_set(struct aml_lcd_drv_s *pdrv, phys_addr_t paddr, unsigned int size);
void lcd_tcon_lut_dma_update_t6x(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_dma_ops_s *ops);

void tcon_sw_dma_mif_set(struct aml_lcd_drv_s *pdrv, phys_addr_t paddr, unsigned int size);
void tcon_sw_dma_trig(struct aml_lcd_drv_s *pdrv);
int tcon_sw_dma_done(struct aml_lcd_drv_s *pdrv);
int wait_tcon_sw_dma_done(struct aml_lcd_drv_s *pdrv, unsigned int timeout_us);
int tcon_aptu_dma_done(struct aml_lcd_drv_s *pdrv);
int wait_tcon_aptu_dma_done(struct aml_lcd_drv_s *pdrv, unsigned int timeout_us);
void tcon_clr_dma_done(struct aml_lcd_drv_s *pdrv);

void lcd_tcon_ext_header_check(struct lcd_tcon_init_block_ext_header_s *ext_header);
int lcd_tcon_get_core_size(struct lcd_tcon_config_s *tcon_conf,
	struct lcd_tcon_init_block_header_s *header,
	struct lcd_tcon_init_block_ext_header_s *ext_header);

int lcd_tcon_valid_check(void);
struct tcon_rmem_s *get_lcd_tcon_rmem(void);
struct tcon_mem_map_table_s *get_lcd_tcon_mm_table(void);
void lcd_tcon_global_reset_t5(struct aml_lcd_drv_s *pdrv);
void lcd_tcon_global_reset_t3(struct aml_lcd_drv_s *pdrv);
void lcd_tcon_global_reset_t3x(struct aml_lcd_drv_s *pdrv);
void lcd_tcon_lut_post_proc(struct aml_lcd_drv_s *pdrv, unsigned int lut_type);
void lcd_tcon_core_reg_set(struct aml_lcd_drv_s *pdrv,
			   struct lcd_tcon_config_s *tcon_conf,
			   struct tcon_mem_map_table_s *mm_table,
			   struct tcon_core_reg_info_s *core_reg_info);
int lcd_tcon_enable_t5(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_disable_t5(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_disable_txhd2(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_top_set_t5(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_top_set_t6d(struct aml_lcd_drv_s *pdrv);

int lcd_tcon_init_setting_check(struct aml_lcd_drv_s *pdrv, struct lcd_detail_timing_s *ptiming,
		struct tcon_core_reg_info_s *core_reg_info);
int lcd_tcon_setting_check_t5(struct aml_lcd_drv_s *pdrv, struct lcd_detail_timing_s *ptiming,
		struct tcon_core_reg_info_s *core_reg_info, char *ferr_str, char *warn_str);
int lcd_tcon_setting_check_t5d(struct aml_lcd_drv_s *pdrv, struct lcd_detail_timing_s *ptiming,
		struct tcon_core_reg_info_s *core_reg_info, char *ferr_str, char *warn_str);

/* common */
void lcd_tcon_mem_sync(struct aml_lcd_drv_s *pdrv, unsigned long paddr, unsigned int mem_size);
unsigned char *lcd_tcon_paddrtovaddr(unsigned long paddr, unsigned int mem_size);
void lcd_tcon_data_multi_bypass_set(struct tcon_mem_map_table_s *mm_table,
				    unsigned int block_type, int flag);
int lcd_tcon_data_multi_init_check(struct aml_lcd_drv_s *pdrv, unsigned short block_type,
		struct lcd_tcon_data_part_ctrl_s *ctrl_part, unsigned char *p,
		unsigned int data_index);
int lcd_tcon_data_common_parse_set(struct aml_lcd_drv_s *pdrv, unsigned char *data_buf,
		phys_addr_t paddr, int init_flag, unsigned int data_index);
void lcd_tcon_init_data_version_update(struct tcon_core_reg_info_s *core_info,
		char *data_buf);
void lcd_tcon_data_block_regen_crc(unsigned char *data);
int lcd_tcon_init_data_update(char *data_buf, struct lcd_detail_timing_s *ptiming);
int lcd_tcon_data_set(struct aml_lcd_drv_s *pdrv, struct tcon_mem_map_table_s *mm_table);
int lcd_tcon_data_load(struct aml_lcd_drv_s *pdrv, unsigned char *data_buf, int index);
void lcd_tcon_complete_data_load(struct aml_lcd_drv_s *pdrv);
void lcd_tcon_reg_table_print(struct tcon_core_reg_info_s *core_reg_info);
void lcd_tcon_reg_readback_print(struct aml_lcd_drv_s *pdrv);
void lcd_tcon_core_info_print(struct aml_lcd_drv_s *pdrv,
		struct tcon_core_reg_info_s *core_reg_info);
void lcd_tcon_axi_rmem_lut_load(struct aml_lcd_drv_s *pdrv, unsigned int index,
				unsigned char *buf, unsigned int size);
#ifdef TCON_DBG_TIME
void lcd_tcon_dbg_trace_clear(void);
void lcd_tcon_dbg_trace_print(void);
#endif

int lcd_tcon_axi_mem_print(struct tcon_rmem_s *tcon_rmem, char *buf, int offset);
void lcd_tcon_debug_file_add(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_local_cfg_s *local_cfg);
void lcd_tcon_debug_file_remove(struct lcd_tcon_local_cfg_s *local_cfg);

int lcd_tcon_mem_od_is_valid(void);
int lcd_tcon_mem_demura_is_valid(void);
unsigned char *lcd_tcon_demura_mem_get(struct aml_lcd_drv_s *pdrv, int *mem_len);
int lcd_tcon_get_table32_reg(struct tcon_core_reg_info_s *core_reg_info,
		unsigned int reg, unsigned int *val);
int lcd_tcon_set_table32_reg(struct tcon_core_reg_info_s *core_reg_info,
		unsigned int reg, unsigned int val);
int lcd_tcon_setb_table32_reg(struct tcon_core_reg_info_s *core_reg_info,
		unsigned int reg, unsigned int val, unsigned int start, unsigned int len);

int lcd_tcon_lock_status_get(struct aml_lcd_drv_s *pdrv);

/*
 * mode: 0-normal mode, 1-fix mode
 */
void tcon_fr_detect_config(struct aml_lcd_drv_s *pdrv, unsigned int mode,
			unsigned int fr_levels[], unsigned int num_level,
			unsigned int step_counter);
void tcon_fr_detect_enable(struct aml_lcd_drv_s *pdrv, int enable);
void lcd_tcon_data_parse_vrr(struct lcd_tcon_vrr_data_s *vrr,
				unsigned char *p, unsigned int data_cnt, unsigned int data_width);
int lcd_tcon_vrr_fr_sw_match(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_vrr_data_s *vrr);

void tcon_i2c_init(struct aml_lcd_drv_s *pdrv, u32 slave_addr, u32 bytes, u32 speed);
unsigned char tcon_ip27_valid(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_ip27_s *ip27);
unsigned int tcon_ip27_calc_trig_line(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_ip27_s *ip27);
void tcon_ip27_init(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_ip27_s *ip27);
void tcon_ip27_probe_init(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_ip27_s *ip27);
void tcon_ip27_setup(struct aml_lcd_drv_s *pdrv);
void tcon_ip27_release(struct aml_lcd_drv_s *pdrv);
/* use rdma, only can be called in vsync isr */
void tcon_ip27_vsync_proc(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_ip27_s *ip27);

#endif
