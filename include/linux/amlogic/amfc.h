/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#ifndef _AMFC_H_
#define _AMFC_H_

#include <linux/amlogic/cpu_version.h>
#define AMFC_S7D				0x37
#define AMFC_T6W                                0x43
#define AMFC_S6					0x48
#define AMFC_T6D				0x49
#define AMFC_T6X				0x4B
#define AMFC_C4					0x4c

#define ALGORITHM_ZSTD				1
#define ALGORITHM_LZ4				2
#define ALGORITHM_LZ4HC				3
#define ALGORITHM_GZIP				4
#define ALGORITHM_ZLIB				5
#define ALGORITHM_DEFLATE			6
#define ALGORITHM_DMA_COPY			15

#define CMD_COMPRESS				1
#define CMD_DECOMPRESS				0

#define AMFC_IRQ_FLAG				(3 << 24)
#define AMFC_IRQ_ERR				BIT(25)
#define AMFC_IRQ_DONE				BIT(24)
#define AMFC_ERR_MASK				(0xff << 8)

#define STATUS_IDLE				0
#define STATUS_BUSY				1
#define STATUS_MASK				1

#define IRQ_MASK				(0x03 << 24)
#define IRQ_DONE				(0x01 << 24)
#define IRQ_ERR					(0x02 << 24)

#define AMFC_ERROR_MASK				(0xff << 8)

#define ADDR_SHIFT				5

/* flags for work mode */
#define AMFC_POLL_MODE				1
#define AMFC_POLL_IRQ_OFF			2

/* error code */
#define AMFC_CMD0_ERR_SRC_SIZE0			0x01
#define AMFC_CMD0_ERR_DST_SIZE0			0x02
#define AMFC_CMD0_ERR_OWNER0			0x03
#define AMFC_CMD0_ERR_ALG			0x04

#define AMFC_CMD1_ERR_SRC_SIZE0			0x09
#define AMFC_CMD1_ERR_DST_SIZE0			0x0a
#define AMFC_CMD1_ERR_OWNER0			0x0b
#define AMFC_CMD1_ERR_ALG			0x0c

#define AMFC_ENC_SRC_PAGE_ERR			0x40
#define AMFC_ENC_DST_PAGE_ERR			0x41
#define AMFC_ENC_DST_SIZE_OVF			0x42

#define AMFC_DEC_SRC_PAGE_ERR			0x80
#define AMFC_DEC_DST_PAGE_ERR			0x81
#define AMFC_DEC_DST_SIZE_OVF			0x82

#define ZSTD_DERR_MAIN_UNDFLOW			0x87
#define ZSTD_DERR_TRI_TYPE_UNDFLOW		0x88
#define ZSTD_DERR_FSEH_LL_NCNT_UNDERFLOW	0x89
#define ZSTD_DERR_FSEH_OF_NCNT_UNDERFLOW	0x8a
#define ZSTD_DERR_FSEH_ML_NCNT_UNDERFLOW	0x8b
#define ZSTD_DERR_FSEH_HUFH_NCNT_UNDERFLOW	0x8c
#define ZSTD_DERR_FSEB_TRI_UNDFLOW		0x8d
#define ZSTD_DERR_HUFB0_UNDFLOW			0x8e
#define ZSTD_DERR_HUFB1_UNDFLOW			0x8f
#define ZSTD_DERR_HUFB2_UNDFLOW			0x90
#define ZSTD_DERR_HUFB3_UNDFLOW			0x91
#define ZSTD_DERR_FRMH_NOT_FOUND		0x92
#define ZSTD_DERR_FRMH_MAGIC_ID			0x93
#define ZSTD_DERR_FRMH_DICT_ID			0x94
#define ZSTD_DERR_BLKH_TYPE			0x95
#define ZSTD_DERR_FSEH_TLOG_HUFH		0x96
#define ZSTD_DERR_FSEH_TLOG_ML			0x97
#define ZSTD_DERR_FSEH_TLOG_OF			0x98
#define ZSTD_DERR_FSEH_TLOG_LL			0x99
#define ZSTD_DERR_FSEH_MAX_HUFH			0x9a
#define ZSTD_DERR_FSEH_MAX_ML			0x9b
#define ZSTD_DERR_FSEH_MAX_OF			0x9c
#define ZSTD_DERR_FSEH_MAX_LL			0x9d
#define ZSTD_DERR_FSEB_HUF_MAXCNT		0x9e
#define ZSTD_DERR_HUFB0_CODE			0x9f
#define ZSTD_DERR_HUFB1_CODE			0xa0
#define ZSTD_DERR_HUFB2_CODE			0xa1
#define ZSTD_DERR_HUFB3_CODE			0xa2
#define ZSTD_DERR_FSEB_OF_BIG			0xa3
#define ZSTD_DERR_FSEB_OF_OOB			0xa4

#define ADDR_SRC				0
#define ADDR_DST				1

#define AMFC_STREAM_MARGIN			64
#define CACHELINE_SIZE				64

#define AMFC_VER_1_0				0x0100
#define AMFC_VER_1_1				0x0101

#define AMFC_IOC_MAGIC				'A'
#define AMFC_MAX_FRAME_SIZE			(4 * 1024 * 1024)

#define AMFC_IOC_COMPRESS			_IOWR(AMFC_IOC_MAGIC, 1, int)
#define AMFC_IOC_DECOMPRESS			_IOWR(AMFC_IOC_MAGIC, 2, int)

#define PAGE_OFF(a)				(((unsigned long)a) & ~PAGE_MASK)

struct amfc_args {
	unsigned long long src_addr;
	unsigned long long dst_addr;
	int src_size;
	int dst_size;
};

enum amfc_page_table {
	TABLE_SRC_COMPRESS = 0,
	TABLE_DST_COMPRESS,
	TABLE_SRC_DECOMPRESS,
	TABLE_DST_DECOMPRESS,
};

struct amfc_cmd_list {
	unsigned int src_addr;
	unsigned int dst_addr;
	unsigned int link_addr;
	unsigned int src_size;
	unsigned int dst_size;
	union {
		unsigned int control;
		struct {
			unsigned src_hash_l  : 8;
			unsigned algorithm   : 4;
			unsigned rsved       : 4;
			unsigned dst_addr_h  : 4;
			unsigned src_addr_h  : 4;
			unsigned end         : 1;
			unsigned dst_scatter : 1;
			unsigned src_scatter : 1;
			unsigned link_mode   : 1;
			unsigned stream      : 1;
			unsigned hash        : 1;
			unsigned compress    : 1;
			unsigned owner       : 1;
		};
	};
	unsigned int status;
	unsigned int result_size;
};

#ifndef __UNCOMPRESS_IMAGE__
struct amfc {
	struct device *dev;
	void *io_base;
	void *clk_base;
	void *bounce_buffer;
	struct amfc_cmd_list *compress;
	struct amfc_cmd_list *decompress;

	struct completion ccomp;	/* compress completion   */
	struct completion dcomp;	/* decompress completion */

	spinlock_t com_lock;		/* lock for compress     */
	spinlock_t dec_lock;		/* lock for decompress   */
	unsigned int hw_version;	/* RTL version of amfc   */

	/*
	 * direct pages for fast quick page mode
	 * 0: src page table for compress
	 * 1: dst page table for compress
	 * 2: src page table for decompress
	 * 3: dst page table for decompress
	 */
	struct page *pages[4];

	/* statistics */
	unsigned long long total_ctick, total_dtick;	/* time ticks for performance */
	unsigned long long z_dtick, e_dtick;
	unsigned long long cin, cout;		/* compressed size            */
	unsigned long long din, z_dout, e_dout;
#ifdef CONFIG_AMLOGIC_F2FS
	unsigned long long f2fs_enc_in, f2fs_enc_out;
	unsigned long long f2fs_dec_in, f2fs_dec_out;
	unsigned long f2fs_enc_fail;
	unsigned long f2fs_dec_fail;
#endif
	unsigned long c_count, d_count;
	unsigned long c_congestion, d_congestion;

	unsigned long rate;			/* hz */
	struct clk *clk;
	int in_dec_err;
	int in_enc_err;
	atomic_t in_suspend;
	unsigned char chip;
	unsigned char work_mode;		/* 0: irq mode, 1: poll mode  */
	unsigned char log;
	unsigned char init_done;
};
#endif

int amfc_init(void);
int amfc_decompress(void *src, void *dst, ssize_t src_size, ssize_t dst_size, int stream);
int amfc_compress(void *src, void *dst, ssize_t src_size, ssize_t dst_size);
struct amfc *get_amfc_instance(void);
int amfc_probe_done(void);

#ifdef __UNCOMPRESS_IMAGE__
void cache_clean_flush(unsigned long start, unsigned long end);
#endif

#ifdef CONFIG_AMLOGIC_F2FS
int hook_f2fs(void);
#endif

static inline int amfc_supported(void)
{
	int cpu = get_cpu_type();

	switch (cpu) {
	case MESON_CPU_MAJOR_ID_T6W:
	case MESON_CPU_MAJOR_ID_S7D:
	case MESON_CPU_MAJOR_ID_S6:
	case MESON_CPU_MAJOR_ID_T6D:
	case MESON_CPU_MAJOR_ID_T6X:
	case MESON_CPU_MAJOR_ID_C4:
	case MESON_CPU_MAJOR_ID_A9:
	case MESON_CPU_MAJOR_ID_C5:
		return 1;
	}
	return 0;
}
#endif
