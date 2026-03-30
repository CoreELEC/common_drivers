/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_DDR_BANDWIDTH_H__
#define __AML_DDR_BANDWIDTH_H__

#define MODE_DISABLE			0
#define MODE_ENABLE			1
#define MODE_AUTODETECT			2

#ifdef ARM
#define DEFAULT_RANGE_LARGE_DDR		0xffffffff
#else
#define DEFAULT_RANGE_LARGE_DDR		0x3ffffffffULL
#endif
#define DEFAULT_THRESHOLD		5000

#define DEFAULT_SAMPLING_FREQ		1000
#define DEFAULT_XTAL_FREQ		24000000UL

#define MAX_DMC_NUM			4
#define DMC_QOS_IRQ			BIT(30)
#define MAX_CHANNEL			24
#define MAX_MAP_ID			256

/* for soc_feature */
#define DEV_IS_MDC		BIT(7)
#define DDR_IS_POLL		BIT(6)
#define DMC_ASYMMETRY		BIT(5)
#define DDR_WIDTH_IS_64BIT	BIT(4)
#define DDR_WIDTH_IS_16BIT	BIT(3)
#define DDR_DEVICE_8BIT		BIT(2)
#define PLL_IS_SEC		BIT(1)

#include "ddr_port.h"
/*
 * register offset for chips before g12
 */
#define DMC_MON_CTRL1			(0x25 << 2)
#define DMC_MON_CTRL2			(0x26 << 2)
#define DMC_MON_CTRL3			(0x27 << 2)
#define DMC_MON_ALL_REQ_CNT		(0x28 << 2)
#define DMC_MON_ALL_GRANT_CNT		(0x29 << 2)
#define DMC_MON_ONE_GRANT_CNT		(0x2a << 2)
#define DMC_MON_SEC_GRANT_CNT		(0x2b << 2)
#define DMC_MON_THD_GRANT_CNT		(0x2c << 2)
#define DMC_MON_FOR_GRANT_CNT		(0x2d << 2)

#define DMC_MON_CTRL4			(0x18 << 2)
#define DMC_MON_CTRL5			(0x19 << 2)
#define DMC_MON_CTRL6			(0x1a << 2)

#define DMC_AM0_CHAN_CTRL		(0x60 << 2)
#define DMC_AM1_CHAN_CTRL		(0x6a << 2)
#define DMC_AM2_CHAN_CTRL		(0x74 << 2)
#define DMC_AM3_CHAN_CTRL		(0x7e << 2)
#define DMC_AM4_CHAN_CTRL		(0x88 << 2)
#define DMC_AM5_CHAN_CTRL		(0x92 << 2)
#define DMC_AM6_CHAN_CTRL		(0x9c << 2)
#define DMC_AM7_CHAN_CTRL		(0xa6 << 2)
#define DMC_AXI0_CHAN_CTRL		(0xb0 << 2)
#define DMC_AXI1_CHAN_CTRL		(0xba << 2)
#define DMC_AXI2_CHAN_CTRL		(0xc4 << 2)
#define DMC_AXI3_CHAN_CTRL		(0xce << 2)
#define DMC_AXI4_CHAN_CTRL		(0xd8 << 2)
#define DMC_AXI5_CHAN_CTRL		(0xe2 << 2)
#define DMC_AXI6_CHAN_CTRL		(0xec << 2)
#define DMC_AXI7_CHAN_CTRL		(0xf6 << 2)

/*
 * register offset for g12a
 */
#define DMC_MON_G12_CTRL0		(0x20  << 2)
#define DMC_MON_G12_CTRL1		(0x21  << 2)
#define DMC_MON_G12_CTRL2		(0x22  << 2)
#define DMC_MON_G12_CTRL3		(0x23  << 2)
#define DMC_MON_G12_CTRL4		(0x24  << 2)
#define DMC_MON_G12_CTRL5		(0x25  << 2)
#define DMC_MON_G12_CTRL6		(0x26  << 2)
#define DMC_MON_G12_CTRL7		(0x27  << 2)
#define DMC_MON_G12_CTRL8		(0x28  << 2)

#define DMC_MON_G12_ALL_REQ_CNT		(0x29  << 2)
#define DMC_MON_G12_ALL_GRANT_CNT	(0x2a  << 2)
#define DMC_MON_G12_ONE_GRANT_CNT	(0x2b  << 2)
#define DMC_MON_G12_SEC_GRANT_CNT	(0x2c  << 2)
#define DMC_MON_G12_THD_GRANT_CNT	(0x2d  << 2)
#define DMC_MON_G12_FOR_GRANT_CNT	(0x2e  << 2)
#define DMC_MON_G12_TIMER		(0x2f  << 2)

#define DMC_AM0_G12_CHAN_CTRL		(0x60 << 2)
#define DMC_AM1_G12_CHAN_CTRL		(0x64 << 2)
#define DMC_AM2_G12_CHAN_CTRL		(0x68 << 2)
#define DMC_AM3_G12_CHAN_CTRL		(0x6c << 2)
#define DMC_AM4_G12_CHAN_CTRL		(0x70 << 2)
#define DMC_AM5_G12_CHAN_CTRL		(0x74 << 2)
#define DMC_AM6_G12_CHAN_CTRL		(0x78 << 2)
#define DMC_AM7_G12_CHAN_CTRL		(0x7c << 2)
#define DMC_AXI0_G12_CHAN_CTRL		(0x80 << 2)
#define DMC_AXI1_G12_CHAN_CTRL		(0x84 << 2)
#define DMC_AXI2_G12_CHAN_CTRL		(0x88 << 2)
#define DMC_AXI3_G12_CHAN_CTRL		(0x8c << 2)
#define DMC_AXI4_G12_CHAN_CTRL		(0x90 << 2)
#define DMC_AXI5_G12_CHAN_CTRL		(0x94 << 2)
#define DMC_AXI6_G12_CHAN_CTRL		(0x98 << 2)
#define DMC_AXI7_G12_CHAN_CTRL		(0x9c << 2)
#define DMC_AXI8_G12_CHAN_CTRL		(0xa0 << 2)
#define DMC_AXI9_G12_CHAN_CTRL		(0xa4 << 2)
#define DMC_AXI10_G12_CHAN_CTRL		(0xa8 << 2)
#define DMC_AXI11_G12_CHAN_CTRL		(0xac << 2)
#define DMC_AXI12_G12_CHAN_CTRL		(0xb0 << 2)

/* data structure */
#define DDR_BANDWIDTH_DEBUG		1
#define DDR_PRIORITY_DEBUG		BIT(31)
#define DDR_PRIORITY_POWER		BIT(30)

#if IS_ENABLED(CONFIG_AMLOGIC_DDR_SSR)
enum ddr_ssr_type {
	ENABLE_SET = 1,
	FMOD_SET,
	AMPLITUDE_SET,
	ENABLE_GET,
	FMOD_GET,
	AMPLITUDE_GET,
};
#endif

enum ddr_type {
	DDR3 = 0,
	DDR4,
	LPDDR4,
	LPDDR3,
	LPDDR2,
	LPDDR4X,
	LPDDR5,
};

enum outstanding_type {
	OUTSTANDING_SET = 1,
	OUTSTANDING_GET,
};

enum property_type {
	WBUF_EMPTY = 1,
	WBUF_H,
	WBUF_M,
	WBUF_L,
	RBUF_H,
	RBUF_M,
	RBUF_L,
};

enum ddr_port_map_type {
	DDR_PORT_ROW_MASTER = 0,
	DDR_PORT_ROW_DESC_SUB,
	DDR_PORT_ROW_VPU_R,
	DDR_PORT_ROW_VPU_W,
};

struct ddr_bandwidth;

struct ddr_grant {
	unsigned long long tick;
	unsigned int all_grant;
	unsigned int all_req;
	unsigned int all_grant16;
	unsigned int channel_grant[MAX_CHANNEL];
};

struct ddr_port_map {
	enum ddr_port_map_type type;
	u8 rw;
	u8 port_id;
	/* MASTER: -1; DESC_SUB: ddr_port_desc.sub_id; VPU_R/W: vpu_sub_list.id */
	int sub_id;
	union {
		struct ddr_port_desc *desc;
		struct vpu_sub_list *vpu_sub;
	};
};

const char *ddr_port_map_name(const struct ddr_port_map *map);

struct ddr_bandwidth_ops {
	void (*init)(struct ddr_bandwidth *db);
	void (*config_port)(struct ddr_bandwidth *db, int channel, int port);
	void (*config_range)(struct ddr_bandwidth *db, int channel,
			     unsigned long start, unsigned long end);
	int  (*handle_irq)(struct ddr_bandwidth *db, struct ddr_grant *dg);
	void (*bandwidth_enable)(struct ddr_bandwidth *db);
	unsigned long (*get_freq)(struct ddr_bandwidth *db);
	void (*bus_width_init)(struct ddr_bandwidth *db);
	int (*outstanding)(struct ddr_bandwidth *db, int bus,
					int value, enum outstanding_type type);
	int (*outstanding_init)(struct ddr_bandwidth *db);
#if DDR_BANDWIDTH_DEBUG
	int (*dump_reg)(struct ddr_bandwidth *db, char *buf);
#endif
	int (*property_access)(struct ddr_bandwidth *db, u64 *val,
			       enum property_type type, int rw);
	int (*side_band)(struct ddr_bandwidth *db, unsigned char dmc, unsigned char bus);
#if IS_ENABLED(CONFIG_AMLOGIC_DDR_SSR)
	int (*ddr_ssr_control)(unsigned char value, enum ddr_ssr_type type);
#endif
	int (*axi_sts_init)(struct ddr_bandwidth *db);
};

struct ddr_bandwidth_sample {
	unsigned long long tick;
	unsigned int total_usage;
	unsigned int total_bandwidth;
	unsigned int bandwidth[MAX_CHANNEL];
};

struct ddr_avg_bandwidth {
	unsigned long long avg_bandwidth;
	unsigned long long avg_usage;
	unsigned long long avg_port[MAX_CHANNEL];
	unsigned long max_bandwidth[MAX_CHANNEL];
	unsigned int sample_count;
};

struct ddr_data_extern {
	char data_bus_width;
	unsigned long ddr_freq;
	unsigned long dmc_freq;
	unsigned int usage_stat[10];
	struct ddr_grant dg;
	struct ddr_bandwidth_sample cur_sample;
	struct ddr_bandwidth_sample max_sample;
	struct ddr_bandwidth_sample prev_sample;
	struct ddr_avg_bandwidth avg;
};

struct bandwidth_addr_range {
	unsigned long start;
	unsigned long end;
};

struct ddr_increase_tool {
	char *buf_addr;
	unsigned int increase_MB;
	unsigned int once_size;
	u64 t_ns;
};

struct outstanding_reg {
	unsigned int offset;
	unsigned int def_val;
};

struct outstanding_level {
	char count;
	int cur_level;
	unsigned int *value;
};

struct ddr_outstanding {
	struct outstanding_level levels;
	struct outstanding_reg *regs;
};

struct ddr_axi_sts {
	unsigned char axi_chan_sts_num;
	unsigned int *axi_chan_sts_offset;
	unsigned int *axi_chan_sts_value;
	unsigned int dmc_chan_sts_offset;
	unsigned int *dmc_chan_sts_value;
};

struct device_id {
	unsigned char id;
	char *name;
};

struct side_band {
	unsigned char flags;
	unsigned char rw;
	unsigned char block_num;
	unsigned char block_bus[32];
	struct mutex lock;			// protect the data of this structure
};

struct bus_devices {
	unsigned char bus;
	unsigned char vpu;
	struct side_band side_band;
	unsigned char num;
	struct device_id *device;
};

struct dmc_bus {
	unsigned char num;
	struct bus_devices *bus;
};

struct mdc_device {
	unsigned int number;
	int port_num;
	void __iomem *reg_base[4];
};

struct bus_width_reg {
	unsigned long addr;
	void __iomem *io_addr;
};

struct ddr_bandwidth {
	unsigned short cpu_type;
	unsigned short real_ports;
	char busy;
	char mode;
	char bytes_per_cycle;
	char soc_feature;		/* some special feature of it */
	int mali_port[2];
	int stat_flag;
	int vpu_bus_num;
	int async_dmc_num;
	struct dmc_bus dmc_bus[4];
	int bus_num;
	int port_map_count;
	unsigned int threshold;
	unsigned int irq_num;
	unsigned int clock_count;
	unsigned int channels;
	unsigned int dmc_number;
	unsigned int usage_stat[10];
	unsigned int bus_width[MAX_DMC_NUM];
	enum ddr_type ddr_type;
	unsigned long ddr_freq;
	unsigned long dmc_freq;
	unsigned long ddr_poll_ns;
	raw_spinlock_t lock;		/* lock for usage statistics */
	unsigned long last_poll_time;
	unsigned long poll_time;
	unsigned int poll_cpu;
	struct hrtimer ddr_poll_timer;
	struct ddr_bandwidth_sample cur_sample;
	struct ddr_bandwidth_sample max_sample;
	struct ddr_bandwidth_sample prev_sample;
	struct ddr_avg_bandwidth avg;
	struct ddr_data_extern data_extern[MAX_DMC_NUM];
	struct bandwidth_addr_range range[MAX_CHANNEL];
	DECLARE_BITMAP(mapid_select, MAX_MAP_ID * MAX_CHANNEL);
	void __iomem *ddr_reg[MAX_DMC_NUM];
	void __iomem *pll_reg;
	void __iomem *freq_reg;
	struct bus_width_reg bus_width_reg[MAX_DMC_NUM];
	struct class *class;
	struct ddr_port_desc *port_desc;
	struct ddr_priority *priority;
	struct vpu_sub_desc *vpu_sub;
	struct ddr_bandwidth_ops *ops;
	struct work_struct work_bandwidth;
	struct ddr_increase_tool increase_tool;
	struct ddr_outstanding ost;
	struct dentry *debugfs;
	struct mdc_device mdc;
	struct ddr_axi_sts axi_sts;
	struct ddr_port_map *port_map_table;
};

unsigned int ddr_get_ch_set_num(int ch);
void ddr_set_map_select(int ch, int map_id);

extern struct ddr_bandwidth *aml_db;
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_G12
extern struct ddr_bandwidth_ops g12_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_GX
extern struct ddr_bandwidth_ops gx_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_GXL
extern struct ddr_bandwidth_ops gxl_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_A1
extern struct ddr_bandwidth_ops a1_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_T5
extern struct ddr_bandwidth_ops t5_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_T7
extern struct ddr_bandwidth_ops t7_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_S4
extern struct ddr_bandwidth_ops s4_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_C3
extern struct ddr_bandwidth_ops c3_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_T5M
extern struct ddr_bandwidth_ops t5m_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_S5
extern struct ddr_bandwidth_ops s5_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_S6
extern struct ddr_bandwidth_ops s6_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_TXHD2
extern struct ddr_bandwidth_ops txhd2_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_S1A
extern struct ddr_bandwidth_ops s1a_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_A4
extern struct ddr_bandwidth_ops a4_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_S7
extern struct ddr_bandwidth_ops s7_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_T6D
extern struct ddr_bandwidth_ops t6d_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_T6W
extern struct ddr_bandwidth_ops t6w_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_T6X
extern struct ddr_bandwidth_ops t6x_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_C4
extern struct ddr_bandwidth_ops c4_ddr_bw_ops;
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_DDR_SSR)
unsigned long ddr_ssr_access(unsigned long addr, unsigned long value, int rw, int is_sec);
static inline unsigned long ddr_ssr_access_sec(unsigned long addr, unsigned long value, int rw)
{
	return ddr_ssr_access(addr, value, rw, 1);
}
#endif

void ddr_poll_start(void *info);
unsigned int aml_get_ddr_usage(void);

int one_dmc_reg_field_access(struct ddr_bandwidth *db, unsigned char dmc, u32 *val, int rw,
			     unsigned int reg, unsigned int offset, unsigned int bits_width);
int all_dmc_reg_field_access(struct ddr_bandwidth *db, u32 *val, int rw,
			     unsigned int reg, unsigned int offset, unsigned int bits_width);
int ddr_axi_sts_alloc(struct ddr_bandwidth *db,
		      unsigned int dmc_chan_sts_offset,
		      unsigned int *axi_chan_sts_offset,
		      unsigned int axi_chan_sts_num);

#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH
int __init ddr_bandwidth_init(void);
void ddr_bandwidth_exit(void);
#else
static int ddr_bandwidth_init(void)
{
	return 0;
}

static void ddr_bandwidth_exit(void)
{
}
#endif

#endif /* __AML_DDR_BANDWIDTH_H__ */
