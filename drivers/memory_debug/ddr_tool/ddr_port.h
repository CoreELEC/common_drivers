/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DDR_PORT_DESC_H__
#define __DDR_PORT_DESC_H__

#define DMC_MON_RW			0x8200004A

#define MAX_NAME			32

/* see cpu_version.h define */
#define DMC_TYPE_M8B			0x1B
#define DMC_TYPE_GXBB			0x1F
#define DMC_TYPE_GXTVBB			0x20
#define DMC_TYPE_GXL			0x21
#define DMC_TYPE_GXM			0x22
#define DMC_TYPE_TXL			0x23
#define DMC_TYPE_TXLX			0x24
#define DMC_TYPE_AXG			0x25
#define DMC_TYPE_GXLX			0x26
#define DMC_TYPE_TXHD			0x27

#define DMC_TYPE_G12A			0x28
#define DMC_TYPE_G12B			0x29
#define DMC_TYPE_GXLX2			0x2A
#define DMC_TYPE_SM1			0x2B
#define DMC_TYPE_A1			0x2C
#define DMC_TYPE_TL1			0x2E
#define DMC_TYPE_TM2			0x2F
#define DMC_TYPE_C1			0x30
#define DMC_TYPE_SC2			0x32
#define DMC_TYPE_C2			0x33
#define DMC_TYPE_T5			0x34
#define DMC_TYPE_T5D			0x35
#define DMC_TYPE_T7			0x36
#define DMC_TYPE_S4			0x37
#define DMC_TYPE_T3			0x38
#define DMC_TYPE_P1			0x39
#define DMC_TYPE_S4D			0x3A
#define DMC_TYPE_T5W			0x3B
#define DMC_TYPE_A5			0x3C
#define DMC_TYPE_C3			0x3D
#define DMC_TYPE_S5			0x3E
#define DMC_TYPE_GXLX3			0x3F
#define DMC_TYPE_A4			0x40
#define DMC_TYPE_T5M			0x41
#define DMC_TYPE_T3X			0x42
#define DMC_TYPE_TXHD2			0x44
#define DMC_TYPE_S1A			0x45
#define DMC_TYPE_S7			0x46
#define DMC_TYPE_S7D			0x47
#define DMC_TYPE_S6			0x48
#define DMC_TYPE_T6D			0x49
#define DMC_TYPE_T6W			0x4A
#define DMC_TYPE_T6X			0x4B
#define DMC_TYPE_C4			0x4C
#define DMC_TYPE_A9			0x50
#define DMC_TYPE_C5			0x4F

#define DMC_READ			0
#define DMC_WRITE			1

/* port defaults sub_id is -1, override with .sub_id = n if needed */
#define PORT_DESC(...)	((struct ddr_port_desc){ .sub_id = -1, .bus = -1, __VA_ARGS__ })

struct ddr_port_desc {
	union {
		struct {
			unsigned int mask3:1;
			unsigned int bus3:7;
			unsigned int mask2:1;
			unsigned int bus2:7;
			unsigned int mask1:1;
			unsigned int bus1:7;
			unsigned int mask0:1;
			unsigned int bus0:7;
		};
		unsigned int bus;
		unsigned int mdc_id;
	};
	unsigned char port_id;
	s16 sub_id;	/* default -1 when using PORT_DESC() */
	unsigned char bus_id;
	unsigned char mdc_bus;
	char port_name[MAX_NAME];
};

struct sub_port_list {
	unsigned char id;
	char *name;
};

struct sub_dev_port {
	int num;
	char port_id; /* save master port id */
	struct sub_port_list *list;
};

struct sub_dev {
	struct sub_dev_port sub_r;
	struct sub_dev_port sub_w;
};

struct sub_dev_desc {
	int num;
	struct sub_dev *subs[];
};

enum ddr_priority_type {
	/* entry set */
	PRIO_NOT_SUPPORT = BIT(0),
	PRIO_AUTO =        BIT(1),
	PRIO_FROM_MDC =    BIT(2),
	PRIO_FROM_NIC =    BIT(3),
	PRIO_FROM_DEV =    BIT(4),
	/* fix only entry can mix set */
	PRIO_FIXED =       BIT(5),
	/* mdc nic dev type set */
	PRIO_REG_RW =      BIT(6),
	PRIO_REG_WO =      BIT(7),
	PRIO_REG_POWER =   BIT(8),
	PRIO_HANDLER =     BIT(9),
};

struct ddr_priority_dev {
	unsigned char port_id;
	signed char sub_id;
	unsigned int type;
	union {
		struct {
			unsigned char fixed_w;
			unsigned char fixed_r;
			unsigned char fixed_nbits;
		};
		struct {
			unsigned long reg_base;
			unsigned char reg_mode;
			long force_mask;
			unsigned long w_offset;
			unsigned long r_offset;
			unsigned char w_lsb;
			unsigned char r_lsb;
			unsigned char w_nbits;
			unsigned char r_nbits;
		};
	};
	/* function pointer to specify handle priority */
	int (*handle_get)(const struct ddr_priority_dev *dev, unsigned int value, int is_w);
	int (*handle_set)(const struct ddr_priority_dev *dev, unsigned int value, int is_w);
};

struct ddr_priority_nic {
	unsigned char port_id;
	signed char sub_id;
	unsigned int type;
	union {
		struct {
			unsigned char fixed_w;
			unsigned char fixed_r;
			unsigned char fixed_nbits;
		};
		struct {
			unsigned long reg_base;
			unsigned char reg_mode;
			unsigned long w_offset;
			unsigned long r_offset;
			unsigned char w_lsb;
			unsigned char r_lsb;
			unsigned char w_nbits;
			unsigned char r_nbits;
			unsigned int r_mask;
			unsigned int w_mask;
		};
	};
};

struct ddr_priority_mdc {
	unsigned char port_id;
	signed char sub_id;
	unsigned int type;
	union {
		struct {
			unsigned char fixed_w;
			unsigned char fixed_r;
			unsigned char fixed_nbits;
		};
		struct {
			unsigned long reg_base;
			unsigned char reg_mode;
			long force_mask;
			unsigned long w_offset;
			unsigned long r_offset;
			unsigned char w_lsb;
			unsigned char r_lsb;
			unsigned char w_nbits;
			unsigned char r_nbits;
		};
	};
};

struct ddr_priority_entry {
	unsigned int type;
	unsigned char port_id;
	unsigned char dev_lsb_r;
	unsigned char dev_lsb_w;
	unsigned char nic_lsb_r;
	unsigned char nic_lsb_w;
	unsigned char mdc_lsb_r;
	unsigned char mdc_lsb_w;

	unsigned char fixed_w;
	unsigned char fixed_r;
	unsigned char fixed_lsb;
	unsigned char fixed_nbits;
};

struct ddr_priority_force {
	unsigned int bus;
	/* reg base fllow aml_ddr ddr_reg[] */
	long r_force_mask;
	long w_force_mask;
	unsigned long r_force_mask_offset;
	unsigned long w_force_mask_offset;
	unsigned long r_offset;
	unsigned long w_offset;
	unsigned char r_lsb;
	unsigned char w_lsb;
	unsigned char r_nbits;
	unsigned char w_nbits;
};

struct ddr_priority_info {
	int dev_num;
	int nic_num;
	int mdc_num;
	int entry_num;
	int dmc_force_num;
	struct ddr_priority_dev *dev;
	struct ddr_priority_nic *nic;
	struct ddr_priority_mdc *mdc;
	struct ddr_priority_entry *entry;
	struct ddr_priority_force *dmc_force;
};

struct dmc_priority {
	unsigned int reg_base;
	unsigned int offset;
	unsigned int force_enable;
	unsigned char bus_id;
	unsigned char w_bit_s;
	unsigned char r_bit_s;
	unsigned char width;
};

struct bus_dev_priority {
	/* default 0: normal mode; 1: secure mode */
	unsigned char reg_mode;
	unsigned char port_id;
	unsigned char bus_id;
	/* priority use bit width:
	 *	0xf: use n bit
	 *	0x7: use 3 bit
	 *	0x4: use 2 bit
	 *	0x1: use 1 bit
	 */
	unsigned char w_bit_s;
	unsigned char r_bit_s;

	unsigned char w_width;
	unsigned char r_width;

	unsigned char w_priority;
	unsigned char r_priority;

	unsigned int reg_base;
	unsigned int w_offset;
	unsigned int r_offset;

	unsigned int force_enable;
	unsigned char num;
	unsigned char share_dev[4];
};

struct demod_priority {
	unsigned char port_id;
	unsigned char allow_access;
	unsigned int reg_base;
	unsigned int mode_reg1;
	unsigned char dtmb_mode_bit;
	unsigned char isdbt_mode_bit;
	unsigned int mode_reg2;
	unsigned char dvb_t2_mode_bit;
	unsigned char *isdbt_name;
	unsigned int isdbt_reg;
	unsigned char isdbt_bit;
	unsigned int dvb_t2_select_reg;
	unsigned int dvb_t2_select_mode;
	unsigned int dvb_t2_reg;
	unsigned char dvb_t2_bit;
	unsigned char *dvb_t2_name;
	unsigned int dtmb_reg;
	unsigned char dtmb_bit;
	unsigned char *dtmb_name;
};

struct demux_priority {
	unsigned int reg;
	unsigned char allow_access;
	unsigned char port_id;
	unsigned char w_bit_s;
	unsigned char r_bit_s;
	unsigned char w_width;
	unsigned char r_width;
};

struct audio_id_name_reg {
	unsigned char id;
	unsigned char ugt;
	unsigned char *name;
	unsigned int reg;
};

struct audio_priority {
	unsigned int reg_base;
	unsigned char allow_access;
	unsigned int num;
	struct audio_id_name_reg audio_x[4];
	unsigned int reg;
	unsigned char port_id;
	unsigned char w_bit_s[4];
	unsigned char r_bit_s[4];
	unsigned char width;
};

struct bcon_priority {
	unsigned int reg;
	unsigned char allow_access;
	unsigned char port_id;
	unsigned char auto_bit;
	unsigned char urgent_bit;
	unsigned char low_level_s;
	unsigned char high_level_s;
	unsigned char level_width;
};

struct id_name {
	unsigned char id;
	unsigned char *name;
};

struct hevc_hcodec_priority {
	unsigned int reg_base;
	unsigned char allow_access;
	unsigned char port_id;

	unsigned char def_w_bit_s;
	unsigned char def_r_bit_s;
	unsigned char def_w_width;
	unsigned char def_r_width;
	unsigned int def_offset;

	unsigned int w_lv1_offset;
	unsigned int w_lv2_offset;
	unsigned int w_lv3_offset;
	unsigned int r_lv1_offset;
	unsigned int r_lv2_offset;
	unsigned int r_lv3_offset;

	unsigned char w_num;
	struct id_name *w_id_name;

	unsigned char r_num;
	struct id_name *r_id_name;
};

struct vpu_super_axi {
	unsigned char axi;
	unsigned char rw;
	unsigned char en_bit;
	unsigned char ur_bit_s;
	unsigned char ur_bit_w;
	unsigned char priority;
	unsigned char enable;
};

struct vpu_super_urgent {
	unsigned int addr;
	unsigned char exist;
	unsigned char sec;
	unsigned char cur;
	unsigned char num;
	struct vpu_super_axi *axi;
};

#define VPU_URGENT_MUX		0	/* 0 enable this item, 1 disable thins item */
#define VPU_URGENT_HANDLED	7	/* */
struct vpu_urgent {
	unsigned int addr;
	unsigned int level;
	unsigned char id;
	unsigned char sec;
	unsigned char ur_bit_s;
	unsigned char ur_bit_w;
	unsigned char flag;
};

struct vpu_top_map {
	unsigned char ur_bit_s;
	unsigned char ur_bit_w;
	unsigned char priority;
	unsigned char num;
	struct vpu_urgent *dev;
	unsigned char id[8];
};

struct vpu_top {
	unsigned char port_id;
	unsigned char axi;
	unsigned char rw;
	unsigned int addr;
	unsigned char sec;
	unsigned char num;
	struct vpu_top_map *map;
};

struct vpu_bit_id_map {
	unsigned char id;
	unsigned char bit;
};

struct vpu_dev_bus_routing {
	unsigned int addr;
	unsigned char sec;
	unsigned char axi;
	unsigned char rw;
	unsigned char idx0;
	unsigned char idx1;
	unsigned char num;
	struct vpu_bit_id_map *map;
};

struct vpu_priority {
	unsigned char allow_access;
	unsigned int reg_base;
	struct vpu_super_urgent *sur;
	unsigned char routing_num;
	struct vpu_dev_bus_routing *routing;
	unsigned char num;
	struct vpu_top *top;
};

struct cur_dmc {
	unsigned char force_enable;
	unsigned char w_priority;
	unsigned char r_priority;
};

struct ddr_priority_v2 {
	unsigned char dmc_num;
	unsigned char bus_dev_num;
	unsigned char cur_bus_dev;
	unsigned int simplify_index;
	char *simplify_buf;
	struct cur_dmc cur_dmc;
	struct dmc_priority *dmc;
	struct bus_dev_priority *bus_dev;
	struct demod_priority *demod;
	struct demux_priority *demux;
	struct audio_priority *audio;
	struct bcon_priority *bcon;
	struct hevc_hcodec_priority *hevc;
	struct hevc_hcodec_priority *hcodec;
	struct vpu_priority *vpu;
};

struct ddr_priority {
	unsigned char ver;
	unsigned int flag;
	unsigned char v1_num;
	ssize_t v1_buf_size;
	char *v1_buf;
	struct ddr_priority_info *v1;
	struct ddr_priority_v2 *v2;
};

struct dev_priority {
	unsigned char w_width;
	unsigned char r_width;
};

ssize_t priority_display(char *buf, loff_t pos, size_t count);
void set_priority_display(const char *buf);
int ddr_priority_init(int cpu_type);
ssize_t ddr_priority_info_show(char *buf);
int ddr_priority_info_write(unsigned int id, int value, int rw, int dmc, char type);
int priority_info_sysfs_parse(const char *buf, size_t count, unsigned int *id,
				     int *val, int *rw, int *dmc, char *type);

int sub_dev_desc_init(int cpu_type, struct sub_dev_desc **desc);
char *sub_dev_to_port(unsigned char port_id, int sid, char rw);
/*
 * This function used only during boot
 */
int ddr_find_port_desc_type(int cpu_type, struct ddr_port_desc **desc, int type);
int ddr_find_port_desc(int cpu_type, struct ddr_port_desc **desc);
unsigned long dmc_rw(unsigned long addr, unsigned long value, int rw);
#endif /* __DDR_PORT_DESC_H__ */
