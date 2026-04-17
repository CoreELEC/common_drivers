/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __RAMDUMP_H__
#define __RAMDUMP_H__

#define SET_REBOOT_REASON           0x82000049

#define AMLOGIC_KERNEL_BOOTED       0x8000
#define RAMDUMP_STICKY_DATA_MASK    0xFFFF
#define RAMDUMP_STICKY_DMA_MASK     0x7F
#define RAMDUMP_DDR_ALIGNED_64MB    0x04000000
#define RAMDUMP_DMA_ALIGNED_4MB     0x00400000
#define RAMDUMP_DMA_MAX_RETRIES     3000
#define RAMDUMP_MD5_DIGEST_SIZE     16
#define RAMDUMP_MD5_STRING_LEN      (RAMDUMP_MD5_DIGEST_SIZE * 2)

noinline void ramdump_sync_data(void);

#endif /* __RAMDUMP_H__ */
