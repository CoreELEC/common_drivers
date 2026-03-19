/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef DI_PQ_H
#define DI_PQ_H

#include <linux/module.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#include <linux/atomic.h>

/************************************************
 * config define
 ***********************************************/
#define MAX_NUM 0xFFFFFF00 // frame_index max,restart not from 0,continuously

#define	DPSS_REG_NUM	9
#define	DPSS_REG_RO_NUM	46
#define	DPSS_DB_DM_NUM	100

struct regs_s {
	u32 addr;
	u32 value;
};

enum dipq_work_mode {
	DI_MODE_SNR	= 0x1,
	DI_MODE_TNR	= 0x2,
	DI_MODE_DCT	= 0x4,
	DI_MODE_MAX	= 0x7FF,
};

enum dipq_debug_mode {
	PQ_MODE_DI	= 0x1,
	PQ_MODE_NR	= 0x2,
	PQ_MODE_FILM	= 0x3,
	PQ_MODE_NRME	= 0x4,
	PQ_MODE_NE	= 0x5,
	PQ_MODE_DEBLOCK	= 0x6,
	PQ_MODE_MNR	= 0x7,
	PQ_MODE_SNR	= 0x8,
	PQ_MODE_TNR1	= 0x9,
	PQ_MODE_TNR2	= 0xa,
	PQ_MODE_TNR3	= 0xb,
	PQ_MODE_TNR4	= 0xc,
	PQ_MODE_TNR5	= 0xd,
	PQ_MODE_TNR6	= 0xe,
	PQ_MODE_TNR7	= 0xf,
	PQ_MODE_TNR8	= 0x10,
	PQ_MODE_FILM2	= 0x11,
	PQ_MODE_MADI	= 0x12,
	PQ_MODE_MADI2	= 0x13,
	PQ_MODE_MCDI	= 0x14,
	PQ_MODE_MCDI2	= 0x15,
	PQ_MODE_DI2	= 0x16,
	PQ_MODE_DI3	= 0x17,
	PQ_MODE_DCT	= 0x18,
	PQ_MODE_DEMO0	= 0x19,
	PQ_MODE_DEMO1	= 0x1a,
	PQ_MODE_DEMO2	= 0x1b,
	PQ_MODE_DEMO3	= 0x1c,
	PQ_MODE_DEMO4	= 0x1d,
	PQ_MODE_DEMO5	= 0x1e,
	PQ_MODE_MAX	= 0x7FF,
};

struct ro_buffer_parm_s {
	u32 buf_id;
	void *buf;
	bool is_update;
	u32 size;//byte number
};

struct di_parm_s {
	u32 ch;
	u32 frame_index; //0 1
	enum dipq_work_mode work_mode;//snr tnr dct
	u32 v_type;
	u16 v_width;
	u16 v_height;
	u16 me_width;
	u16 me_height;
	u32 v_bitdepth;
	u32 is_update;//dae down true,
	struct ro_buffer_parm_s *dae_ro_buffer;
	struct ro_buffer_parm_s *dpe_ro_buffer;
	struct ro_buffer_parm_s *dblk_h_ro_buffer;
	struct ro_buffer_parm_s *dblk_v_ro_buffer;
	struct ro_buffer_parm_s *me_ro_buffer;
	struct ro_buffer_parm_s *input_ro_buffer;
	struct ro_buffer_parm_s *dae_ro_pd_buffer;
	void *vaddr_ro1;
	void *vaddr_ro2;
	void *vaddr_ro3;
	void *vaddr_ro4;
	u32 size_ro1;
	u32 size_ro2;
	u32 size_ro3;
	u32 size_ro4;
	u32 v_sig_fmt;//secam
	bool vaddr_null;
	bool is_vdin;//HDMI IN
	const char *checksum;//30
	u32 bitdepth;//0~8bit,1~10bit,2~12bit
	u32 reserved1;
	u32 reserved2;
	u32 reserved3;
	u32 reserved4;
	u32 reserved5;
	u32 reserved6;
	atomic_t pq_dae_unreg;
	atomic_t pq_dpe_unreg;
};

struct reg_acc {
	void (*wr)(unsigned int adr, unsigned int val);
	unsigned int (*rd)(unsigned int adr);
	unsigned int (*bwr)(unsigned int adr, unsigned int val,
			    unsigned int start, unsigned int len);
	unsigned int (*brd)(unsigned int adr, unsigned int start,
			    unsigned int len);
};

struct di_db_reg_s {
	unsigned int reg_addr;
	unsigned char bit_start;
	unsigned char bit_end;
	int val;
};

extern struct di_db_reg_s di_db_dm[DPSS_DB_DM_NUM];//N+6
extern struct di_db_reg_s di_db_dct[DPSS_DB_DM_NUM];

typedef int (*di_dae_hw_init_out)(struct di_parm_s *pdi_parm, const struct reg_acc *op);
typedef int (*di_dpe_hw_init_out)(struct di_parm_s *pdi_parm, const struct reg_acc *op);

typedef int (*di_dae_read_fwalg_out)(struct di_parm_s *pdi_parm, const struct reg_acc *op);
typedef int (*di_dae_write_fwalg_out)(struct di_parm_s *pdi_parm, const struct reg_acc *op);
typedef int (*di_dae_fwalg_wrap_out)(struct di_parm_s *pdi_parm, const struct reg_acc *op);

typedef int (*di_dpe_read_fwalg_out)(struct di_parm_s *pdi_parm, const struct reg_acc *op);
typedef int (*di_dpe_write_fwalg_out)(struct di_parm_s *pdi_parm, const struct reg_acc *op);
typedef int (*di_dpe_fwalg_wrap_out)(struct di_parm_s *pdi_parm, const struct reg_acc *op);

typedef int (*pq_create_instance_out)(u8 ch);
typedef int (*pq_destroy_instance_out)(u8 ch);

typedef int (*dpss_di_param_show_out)(enum dipq_debug_mode dbg_type, char *buf);
typedef int (*dpss_di_param_store_out)(enum dipq_debug_mode dbg_type, char *buf, size_t count);

/**************************************
 * @DAE/APE
 * @step1:create_instance hw init
 * @step2:READ--WRITE--DAE(N)--READ--ALG--WRITE--DAE(N+1)--READ--ALG--WRITE--DAE(N+2)
 * @dae_hw_init
 * @READ:   dae_fwalg_read
 * @WRITE:  dae_fwalg_write
 * @ALG:    dae_fwalg
 * @step3:destroy_instance
 **************************************/
struct di_ext_pq_ops {
	pq_create_instance_out pq_create_instance; //create channel
	pq_destroy_instance_out pq_destroy_instance;//destroy channel

	di_dae_hw_init_out dae_hw_init; //dae algorithm hw init power on
	di_dae_read_fwalg_out dae_read_fwalg; //dae algorithm read + frame
	di_dae_write_fwalg_out dae_write_fwalg;//dae algorithm write timing + frame
	di_dae_fwalg_wrap_out fwalg_dae; //do dae algorithm

	di_dpe_hw_init_out dpe_hw_init; //dpe algorithm hw init power on
	di_dpe_read_fwalg_out dpe_read_fwalg; //dpe algorithm read
	di_dpe_write_fwalg_out dpe_write_fwalg;//dpe algorithm write
	di_dpe_fwalg_wrap_out fwalg_dpe; //do dpe algorithm
	dpss_di_param_show_out dpss_di_param_show; //do dinr_param
	dpss_di_param_store_out dpss_di_param_store; //do dinr_param
};

int RegisterDI_Function(struct di_ext_pq_ops *func, const char *ver);
int UnRegisterDI_Function(struct di_ext_pq_ops *func);
int pq_create_instance(u8 ch);
int pq_destroy_instance(u8 ch);
void di_attach_to_pq(void);
int dae_hw_init(struct di_parm_s *pdi_parm, const struct reg_acc *op);
int dae_fwalg_read(struct di_parm_s *pdi_parm, const struct reg_acc *op);
int dae_fwalg_write(struct di_parm_s *pdi_parm, const struct reg_acc *op);
int dae_fwalg(struct di_parm_s *pdi_parm, const struct reg_acc *op);
int dpe_hw_init(struct di_parm_s *pdi_parm, const struct reg_acc *op);
int dpe_fwalg_read(struct di_parm_s *pdi_parm, const struct reg_acc *op);
int dpe_fwalg_write(struct di_parm_s *pdi_parm, const struct reg_acc *op);
int dpe_fwalg(struct di_parm_s *pdi_parm, const struct reg_acc *op);
ssize_t dpss_pq_di_show(const struct class *class,
	const struct class_attribute *attr, char *buf);
ssize_t dpss_pq_di_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count);
ssize_t dpss_pq_di2_show(const struct class *class,
	const struct class_attribute *attr, char *buf);
ssize_t dpss_pq_di2_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count);
ssize_t dpss_pq_di3_show(const struct class *class,
	const struct class_attribute *attr, char *buf);
ssize_t dpss_pq_di3_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count);
ssize_t dpss_pq_nr_show(const struct class *class,
	const struct class_attribute *attr, char *buf);
ssize_t dpss_pq_nr_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count);
ssize_t dpss_pq_film_show(const struct class *class,
	const struct class_attribute *attr, char *buf);
ssize_t dpss_pq_film_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count);
ssize_t dpss_pq_film2_show(const struct class *class,
	const struct class_attribute *attr, char *buf);
ssize_t dpss_pq_film2_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count);
ssize_t dpss_pq_nrme_show(const struct class *class,
	const struct class_attribute *attr, char *buf);
ssize_t dpss_pq_nrme_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count);
ssize_t dpss_pq_ne_show(const struct class *class,
	const struct class_attribute *attr, char *buf);
ssize_t dpss_pq_ne_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count);
ssize_t dpss_pq_deblock_show(const struct class *class,
	const struct class_attribute *attr, char *buf);
ssize_t dpss_pq_deblock_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count);
ssize_t dpss_pq_mnr_show(const struct class *class,
	const struct class_attribute *attr, char *buf);
ssize_t dpss_pq_mnr_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count);
ssize_t dpss_pq_snr_show(const struct class *class,
	const struct class_attribute *attr, char *buf);
ssize_t dpss_pq_snr_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count);
ssize_t dpss_pq_tnr1_show(const struct class *class,
	const struct class_attribute *attr, char *buf);
ssize_t dpss_pq_tnr1_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count);
ssize_t dpss_pq_tnr2_show(const struct class *class,
	const struct class_attribute *attr, char *buf);
ssize_t dpss_pq_tnr2_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count);
ssize_t dpss_pq_tnr3_show(const struct class *class,
	const struct class_attribute *attr, char *buf);
ssize_t dpss_pq_tnr3_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count);
ssize_t dpss_pq_tnr4_show(const struct class *class,
	const struct class_attribute *attr, char *buf);
ssize_t dpss_pq_tnr4_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count);
ssize_t dpss_pq_tnr5_show(const struct class *class,
	const struct class_attribute *attr, char *buf);
ssize_t dpss_pq_tnr5_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count);
ssize_t dpss_pq_tnr6_show(const struct class *class,
	const struct class_attribute *attr, char *buf);
ssize_t dpss_pq_tnr6_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count);
ssize_t dpss_pq_tnr7_show(const struct class *class,
	const struct class_attribute *attr, char *buf);
ssize_t dpss_pq_tnr7_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count);
ssize_t dpss_pq_tnr8_show(const struct class *class,
	const struct class_attribute *attr, char *buf);
ssize_t dpss_pq_tnr8_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count);
ssize_t dpss_pq_madi_show(const struct class *class,
	const struct class_attribute *attr, char *buf);
ssize_t dpss_pq_madi_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count);
ssize_t dpss_pq_madi2_show(const struct class *class,
	const struct class_attribute *attr, char *buf);
ssize_t dpss_pq_madi2_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count);
ssize_t dpss_pq_mcdi_show(const struct class *class,
	const struct class_attribute *attr, char *buf);
ssize_t dpss_pq_mcdi_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count);
ssize_t dpss_pq_mcdi2_show(const struct class *class,
	const struct class_attribute *attr, char *buf);
ssize_t dpss_pq_mcdi2_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count);
ssize_t dpss_pq_dct_show(const struct class *class,
	const struct class_attribute *attr, char *buf);
ssize_t dpss_pq_dct_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count);
#endif /* DI_PQ_H */
