/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_SD_H__
#define __AML_SD_H__

#include <linux/mmc/card.h>
#include <linux/interrupt.h>
/* unknown */
#define CARD_TYPE_UNKNOWN		0
/* MMC card */
#define CARD_TYPE_MMC			1
/* SD card */
#define CARD_TYPE_SD			2
/* SDIO card */
#define CARD_TYPE_SDIO			3
/* SD combo (IO+mem) card */
#define CARD_TYPE_SD_COMBO		4
/* NON sdio device (means SD/MMC card) */
#define CARD_TYPE_NON_SDIO		5

#define aml_card_type_unknown(c)	((c)->card_type == CARD_TYPE_UNKNOWN)
#define aml_card_type_mmc(c)		((c)->card_type == CARD_TYPE_MMC)
#define aml_card_type_sd(c)		((c)->card_type == CARD_TYPE_SD)
#define aml_card_type_sdio(c)		((c)->card_type == CARD_TYPE_SDIO)
#define aml_card_type_non_sdio(c)	((c)->card_type == CARD_TYPE_NON_SDIO)

#define EMMC_CMD_FALLING_SML BIT(0)
#define EMMC_CMD_RISING_SML BIT(1)
#define EMMC_CMD_CORE_CLK_SML BIT(2)
#define EMMC_CMD_SD_CLK_SML BIT(3)

#define EMMC_CMD_LINE_DELAY_MODE BIT(0)
#define EMMC_CMD_RX_DELAY_MODE BIT(1)

/* flag is "AML" */
#define TUNED_FLAG            0x004C4D41
/* version is "V4" */
#define TUNED_VERSION         0x00003456
/* magic is 0x00487e44 */
#define TUNED_MAGIC           0x00487e44

struct mmc_phase {
	unsigned int core_phase;
	unsigned int tx_phase;
	unsigned int rx_phase;
	unsigned int tx_delay;
};

struct para_e {
	struct mmc_phase init;
	struct mmc_phase hs;
	struct mmc_phase hs2;
	struct mmc_phase hs4;
	struct mmc_phase sdr;
};

#define LATCHING_RXPHASE    0
#define LATCHING_TXPHASE    1
#define LATCHING_FIXADJ     2
#define MMC_HOST_V8         8
#define MMC_HOST_VERSION(h)        ((h)->data->version)

#define MMC_SRAM_DESC_BUF_OFF(h)   ((h)->data->sram_off)

#define MMC_SRAM_DATA_BUF_OFF(h)   ((h)->data->data_off)
#define MMC_SRAM_DATA_BUF_LEN(h)   ((h)->data->data_size)
struct meson_mmc_data {
	unsigned int tx_delay_mask;
	unsigned int rx_delay_mask;
	unsigned int always_on;
	unsigned int adjust;
	u8 latching_mode;
	u8 version;
	unsigned int sram_off;
	unsigned int data_off;
	unsigned int data_size;
};

enum aml_host_bus_fsm { /* Host bus fsm status */
	BUS_FSM_IDLE,           /* 0, idle */
	BUS_FSM_SND_CMD,        /* 1, send cmd */
	BUS_FSM_CMD_DONE,       /* 2, wait for cmd done */
	BUS_FSM_RESP_START,     /* 3, resp start */
	BUS_FSM_RESP_DONE,      /* 4, wait for resp done */
	BUS_FSM_DATA_START,     /* 5, data start */
	BUS_FSM_DATA_DONE,      /* 6, wait for data done */
	BUS_FSM_DESC_WRITE_BACK,/* 7, wait for desc write back */
	BUS_FSM_IRQ_SERVICE,    /* 8, wait for irq service */
};

struct sd_emmc_desc {
	u32 cmd_cfg;
	u32 cmd_arg;
	u32 cmd_data;
	u32 cmd_resp;
};

#define SG_VALID            BIT(0)
#define SG_EOC              BIT(1)
#define SG_LENGTH_MASK      GENMASK(31, 16)
#define SG_LENGTH_MAX       (64 * 1024)

struct hs400_para {
	unsigned int delay1;
	unsigned int delay2;
	unsigned int intf3;
	unsigned int flag;
};

struct aml_tuning_para {
	unsigned char chip_id[16];
	unsigned int magic;
	unsigned int clk_rate;
	struct hs400_para hs4[10];
	unsigned int version;
	int temp_index;
	u64 checksum;
};

struct mmc_stat_t {
	u64 r_io_cnt;
	u64 r_total_cnt;
	u64 w_io_cnt;
	u64 w_total_cnt;
};

struct meson_host {
	struct device		*dev;
	struct meson_mmc_data *data;
	struct mmc_host	*mmc;
	struct mmc_command	*cmd;
	struct delayed_work dtbkey;
	void __iomem *regs;
	void __iomem *pin_mux_base;
	void __iomem *clk_tree_base;
	struct resource *res[3];
	struct clk *core_clk;
	struct clk *mmc_clk;
	struct clk *mux[3];
	struct clk *mux_div;
	struct clk *clk[3];
	unsigned long req_rate;
	unsigned char timing;
	bool ddr;

	bool dram_access_quirk;

	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_clk_gate;
	struct pinctrl_state *pins_sleep;

	unsigned int bounce_buf_size;
	void *bounce_buf;
	dma_addr_t bounce_dma_addr;
	struct sd_emmc_desc *descs;
	dma_addr_t descs_dma_addr;
	unsigned int desc_buf_size;
	u32 *sg_descs;
	dma_addr_t sg_descs_dma_addr;

	int irq;

	bool vqmmc_enabled;
	struct para_e sd_mmc;
	char is_tuning;
	bool needs_pre_post_req;
	int sd_sdio_switch_volat_done;
	int irq_sdio_sleep;
	int sdio_irqen;
	int debug_flag;
	unsigned int card_type;
	u8 *blk_test;
	u8 *adj_win;
	void (*pre_dma)(struct mmc_host *mmc, u32 cmd_cfg, struct mmc_command *cmd);
	int first_temp_index;
	int cur_temp_index;
	int compute_cmd_delay;
	int compute_coef;
	unsigned int save_para;
	unsigned int src_clk_rate;
	struct aml_tuning_para para;
	int run_pxp_flag;
	int nwr_cnt;
	bool auto_clk;
	bool ignore_desc_busy;
	bool use_intf3_tuning;
	bool enable_hwcq;
	bool enable_inline_crypto;
	int flags;
	spinlock_t lock; /* lock for claim and bus ops */
	bool sd_clk_sample;
	struct dentry *debugfs_root;
	struct clk *src_clk;
	unsigned int vendor_id;
	struct mmc_stat_t mmc_stat;
	bool mmc_stat_debug;
	unsigned int cmd_dly;
	unsigned int dat0_dly;
	unsigned int dat1_dly;
	unsigned int dat2_dly;
	unsigned int dat3_dly;
	unsigned int req_size;
	int run_low_mem;
	int reserved_out;
	u32 reg_bak[20];
	struct mmc_card *tuning_card;
};

struct wifi_clk_table {
	char m_wifi_name[20];
	unsigned short m_use_flag;
	unsigned short m_device_id;
	unsigned int m_uhs_max_dtr;
};

enum wifi_clk_table_e {
	WIFI_CLOCK_TABLE_8822BS = 0,
	WIFI_CLOCK_TABLE_8822CS = 1,
	WIFI_CLOCK_TABLE_QCA6174 = 2,
	WIFI_CLOCK_TABLE_8822ES = 3,
	WIFI_CLOCK_TABLE_MAX,
};

#if IS_ENABLED(CONFIG_AMLOGIC_MMC_MESON_GX)
void sdio_reinit(void);
const char *get_wifi_inf(void);
int sdio_get_vendor(void);
#else /* !CONFIG_AMLOGIC_MMC_MESON_GX */
void sdio_reinit(void)
{
}

const char *get_wifi_inf(void)
{
	return NULL;
}

int sdio_get_vendor(void)
{
	return 0;
}
#endif
void aml_host_bus_fsm_show(struct mmc_host *mmc, int status);
extern struct mmc_host *sdio_host;
int mmc_io_rw_direct(struct mmc_card *card, int write, unsigned int fn,
	unsigned int addr, u8 in, u8 *out);

#define   DRIVER_NAME "meson-gx-mmc"

#define	  SD_EMMC_CLOCK 0x0
#define   CLK_DIV_MASK GENMASK(5, 0)
#define   CLK_SRC_MASK GENMASK(7, 6)
#define   CLK_CORE_PHASE_MASK GENMASK(9, 8)
#define   CLK_TX_PHASE_MASK GENMASK(11, 10)
#define   CLK_RX_PHASE_MASK GENMASK(13, 12)
#define   CLK_PHASE_0 0
#define   CLK_PHASE_180 2

#define   CLK_V3_TX_DELAY_MASK GENMASK(21, 16)
#define   CLK_V3_RX_DELAY_MASK GENMASK(27, 22)
#define   CLK_V3_ALWAYS_ON BIT(28)
#define   CFG_IRQ_SDIO_SLEEP BIT(29)
#define   CFG_IRQ_SDIO_SLEEP_DS BIT(30)

#define   CLK_TX_DELAY_MASK(h)    ((h)->data->tx_delay_mask)
#define   CLK_RX_DELAY_MASK(h)    ((h)->data->rx_delay_mask)
#define   CLK_ALWAYS_ON(h)        ((h)->data->always_on)

#define SD_EMMC_DELAY1 0x4
#define SD_EMMC_DELAY2 0x8
#define   DELAY2_CMD_MASK GENMASK(29, 24)
#define SD_EMMC_V3_ADJUST 0xc
#define	  CALI_SEL_MASK GENMASK(11, 8)
#define	  CALI_ENABLE BIT(12)
#define	  CFG_ADJUST_ENABLE BIT(13)
#define	  CALI_RISE BIT(14)
#define	  DS_ENABLE BIT(15)
#define	  CLK_ADJUST_DELAY GENMASK(21, 16)
#define	  ADJ_AUTO BIT(22)

#define SD_EMMC_CALOUT 0x10
#define SD_EMMC_ADJ_IDX_LOG 0x20
#define SD_EMMC_CLKTEST_LOG 0x24
#define   CLKTEST_TIMES_MASK GENMASK(30, 0)
#define   CLKTEST_DONE BIT(31)
#define SD_EMMC_CLKTEST_OUT 0x28
#define SD_EMMC_EYETEST_LOG 0x2c
#define   EYETEST_TIMES_MASK GENMASK(30, 0)
#define   EYETEST_DONE BIT(31)
#define SD_EMMC_EYETEST_OUT0 0x30
#define SD_EMMC_EYETEST_OUT1 0x34
#define SD_EMMC_INTF3 0x38
#define   CLKTEST_EXP_MASK GENMASK(4, 0)
#define   CLKTEST_ON_M BIT(5)
#define   EYETEST_EXP_MASK GENMASK(10, 6)
#define   EYETEST_ON BIT(11)
#define   DS_SHT_M_MASK GENMASK(17, 12)
#define   DS_SHT_EXP_MASK GENMASK(21, 18)
#define   SD_INTF3 BIT(22)

#define   EYETEST_SEL BIT(26)
#define   RESP_SEL BIT(27)
#define   CFG_RX_SEL BIT(26)
#define   CFG_RX_PN BIT(27)
#define   RESP_OLD BIT(28)
#define   RESP_PN BIT(29)
#define   RESP_DS BIT(30)

#define SD_EMMC_ADDR64 0xA0
#define   DESC_ADDR64_MASK GENMASK(7, 0)
#define   DATA_ADDR64_MASK GENMASK(15, 8)
#define   RESP_ADDR64_MASK GENMASK(23, 16)
#define   BUS64 BIT(24)

#define SD_EMMC_START 0x40
#define   START_DESC_INIT BIT(0)
#define   START_DESC_BUSY BIT(1)
#define   START_DESC_ADDR_MASK GENMASK(31, 2)

#define SD_EMMC_CFG 0x44
#define   CFG_BUS_WIDTH_MASK GENMASK(1, 0)
#define   CFG_BUS_WIDTH_1 0x0
#define   CFG_BUS_WIDTH_4 0x1
#define   CFG_BUS_WIDTH_8 0x2
#define   CFG_DDR BIT(2)
#define   CFG_BLK_LEN_MASK GENMASK(7, 4)
#define   CFG_RESP_TIMEOUT_MASK GENMASK(11, 8)
#define   CFG_RC_CC_MASK GENMASK(15, 12)
#define   CFG_STOP_CLOCK BIT(22)
#define   CFG_CLK_ALWAYS_ON BIT(18)
#define   CFG_CHK_DS BIT(20)
#define   CFG_AUTO_CLK BIT(23)
#define   CFG_ERR_ABORT BIT(27)

#define SD_EMMC_STATUS 0x48
#define   STATUS_BUSY BIT(31)
#define   STATUS_DESC_BUSY BIT(30)
#define   STATUS_DATI GENMASK(23, 16)

#define SD_EMMC_IRQ_EN 0x4c
#define   IRQ_RXD_ERR_MASK GENMASK(7, 0)
#define   IRQ_TXD_ERR BIT(8)
#define   IRQ_DESC_ERR BIT(9)
#define   IRQ_RESP_ERR BIT(10)
#define   IRQ_CRC_ERR \
	(IRQ_RXD_ERR_MASK | IRQ_TXD_ERR | IRQ_DESC_ERR | IRQ_RESP_ERR)
#define   IRQ_RESP_TIMEOUT BIT(11)
#define   IRQ_DESC_TIMEOUT BIT(12)
#define   IRQ_TIMEOUTS \
	(IRQ_RESP_TIMEOUT | IRQ_DESC_TIMEOUT)
#define   IRQ_END_OF_CHAIN BIT(13)
#define   IRQ_RESP_STATUS BIT(14)
#define   IRQ_SDIO BIT(15)
#define   CFG_CMD_SETUP BIT(17)
#define   BUS_FSM_MASK GENMASK(29, 26)
#define   IRQ_EN_MASK \
	(IRQ_CRC_ERR | IRQ_TIMEOUTS | IRQ_END_OF_CHAIN | IRQ_RESP_STATUS |\
	 IRQ_SDIO)

#define SD_EMMC_CMD_CFG 0x50
#define SD_EMMC_CMD_ARG 0x54
#define SD_EMMC_CMD_DAT 0x58
#define SD_EMMC_CMD_RSP 0x5c
#define SD_EMMC_CMD_RSP1 0x60
#define SD_EMMC_CMD_RSP2 0x64
#define SD_EMMC_CMD_RSP3 0x68

#define SD_EMMC_CURR_CFG 0x70
#define SD_EMMC_NEXT_CFG 0x80

#define SD_EMMC_RXD 0x94
#define SD_EMMC_TXD 0x94
#define SD_EMMC_LAST_REG SD_EMMC_TXD

#define SD_EMMC_SRAM_DESC_BUF_OFF 0x200
#define SD_EMMC_SRAM_DATA_BUF_OFF 0x400
#define SD_EMMC_SRAM_DATA_BUF_LEN 1024
#define SD_EMMC_SRAM_DESC_BUF_OFF_V8 0x400
#define SD_EMMC_SRAM_DATA_BUF_OFF_V8 0x800
#define SD_EMMC_SRAM_DATA_BUF_LEN_V8 2048

#define SD_EMMC_MAX_REQ_SIZE (256 * 1024)
#define SD_EMMC_MAX_REQ_SIZE_LOW_MEM (128 * 1024)
#define SD_EMMC_MAX_SEG_SIZE (64 * 1024)
#define SD_EMMC_DESC_MAX_BLKS 511
#define SD_EMMC_DESC_MAX_SIZE (511 * 512)

#define SD_EMMC_CFG_BLK_SIZE 512 /* internal buffer max: 512 bytes */
#define SD_EMMC_CFG_RESP_TIMEOUT 256 /* in clock cycles */
#define SD_EMMC_CMD_TIMEOUT 1024 /* in ms */
#define SD_EMMC_CMD_TIMEOUT_DATA 4096 /* in ms */
#define SD_EMMC_CFG_CMD_GAP 16 /* in clock cycles */
#define SD_EMMC_DESC_BUF_LEN (3 * PAGE_SIZE)
#define SD_EMMC_MAX_SEGS     ((SD_EMMC_DESC_BUF_LEN / 16) - 1)
#define SD_EMMC_MAX_SEGS_LOW_MEM   256

#define SD_EMMC_PRE_REQ_DONE BIT(0)
#define SD_EMMC_DESC_CHAIN_MODE BIT(1)

#define MUX_CLK_NUM_PARENTS 2

#define CMD_CFG_LENGTH_MASK GENMASK(8, 0)
#define CMD_CFG_BLOCK_MODE BIT(9)
#define CMD_CFG_R1B BIT(10)
#define CMD_CFG_END_OF_CHAIN BIT(11)
#define CMD_CFG_TIMEOUT_MASK GENMASK(15, 12)
#define CMD_CFG_NO_RESP BIT(16)
#define CMD_CFG_NO_CMD BIT(17)
#define CMD_CFG_DATA_IO BIT(18)
#define CMD_CFG_DATA_WR BIT(19)
#define CMD_CFG_RESP_NOCRC BIT(20)
#define CMD_CFG_RESP_128 BIT(21)
#define CMD_CFG_RESP_NUM BIT(22)
#define CMD_CFG_DATA_NUM BIT(23)
#define CMD_CFG_CMD_INDEX_MASK GENMASK(29, 24)

#define CMD_CFG_LINK  BIT(30)
#define CMD_CFG_OWNER BIT(31)

#define CMD_DATA_MASK GENMASK(31, 2)
#define CMD_DATA_BIG_ENDIAN BIT(1)
#define CMD_DATA_SRAM BIT(0)
#define CMD_RESP_MASK GENMASK(31, 1)
#define CMD_RESP_SRAM BIT(0)
#define EMMC_SDIO_CLOCK_FELD    0Xffff
#define CALI_HS_50M_ADJUST      0
#define ERROR   1
#define FIXED   2
#define	MMC_PATTERN_NAME		"pattern"
#ifdef CONFIG_AMLOGIC_MIN_RESERVED_PARTITION
#define	MMC_PATTERN_OFFSET		((4 * SZ_1M + 528 * SZ_1K) / 512)
#else
#define	MMC_PATTERN_OFFSET		(((36 + 3) * SZ_1M) / 512)
#endif
#define	MMC_PATTERN_SIZE		(256 * 512)
#define	MMC_MAGIC_NAME			"magic"
#ifdef CONFIG_AMLOGIC_MIN_RESERVED_PARTITION
#define	MMC_MAGIC_OFFSET		((4 * SZ_1M + 656 * SZ_1K) / 512)
#else
#define	MMC_MAGIC_OFFSET		(((36 + 6) * SZ_1M) / 512)
#endif
#define	MMC_MAGIC_SIZE			(256 * 512)
#define	MMC_RANDOM_NAME			"random"
#ifdef CONFIG_AMLOGIC_MIN_RESERVED_PARTITION
#define	MMC_RANDOM_OFFSET		((4 * SZ_1M + 784 * SZ_1K) / 512)
#else
#define	MMC_RANDOM_OFFSET		(((36 + 7) * SZ_1M) / 512)
#endif
#define	MMC_RANDOM_SIZE			(256 * 512)
#define	MMC_DTB_NAME			"dtb"
#ifdef CONFIG_AMLOGIC_MIN_RESERVED_PARTITION
#define	MMC_DTB_OFFSET			((4 * SZ_1M + 912 * SZ_1K) / 512)
#else
#define	MMC_DTB_OFFSET			(((36 + 4) * SZ_1M) / 512)
#endif
#define CALI_BLK_CNT	80
#define CALI_HS_50M_ADJUST	0
#define EMMC_SDIO_CLOCK_FELD	0Xffff
#define MMC_PM_TIMEOUT	(2000)
#define ERROR	1
#define FIXED	2
#define RETUNING	3
#define	DATA3_PINMUX_MASK GENMASK(15, 12)

#define TUNING_NUM_PER_POINT 40
#define MAX_TUNING_RETRY 4
#define AML_FIXED_ADJ_MAX 6
#define AML_FIXED_ADJ_MIN 5
#define AML_FIXADJ_STEP 4
#define ADJ_WIN_PRINT_MAXLEN 256
#define NO_FIXED_ADJ_MID BIT(31)
#define AML_MV_DLY2_NOMMC_CMD(x) ((x) << 24)

#define SD_EMMC_FIXED_ADJ_HS200
#define EMMC_CMD_WIN_MAX_SIZE	50
#define EMMC_CMD_WIN_FULL_SIZE	64
#define CMD_TUNING_RETRIES 6

#define DBG_COMMON        BIT(0)
#define DBG_HS200         BIT(1)
#define Print_dbg(dbg_level, fmt, args...) do {\
		if ((dbg_level) & mmc_debug)	\
			pr_info("[%s]" fmt, __func__, ##args);	\
} while (0)

/* delay_cell=70ps,1ns/delay_cell */
#define DELAY_CELL_COUNTS 14

/* emmc partition */
#define MMC_TUNING_PARA_RESERVED	(10 * SZ_1M)

#define RESULT_OK                       0
#define RESULT_FAIL                     1
#define RESULT_UNSUP_HOST               2
#define RESULT_UNSUP_CARD               3

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
/* Host attributes */
#define AML_MMC_64BIT_DMA     BIT(0)
#endif

/* Set to 1 for no timeout */
#define SD_EMMC_CMD_NO_TIMEOUT 1
/* Set to 32.768s for max timeout */
#define SD_EMMC_CMD_MAX_TIMEOUT (32768)
/* Port from block.c */
#define MMC_EXTRACT_INDEX_FROM_ARG(x) (((x) & 0x00FF0000) >> 16)

#endif /*__AML_SD_H__*/

