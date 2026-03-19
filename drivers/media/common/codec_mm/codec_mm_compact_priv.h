/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _CODEC_MM_COMPACT_H
#define _CODEC_MM_COMPACT_H

void range_conflict_check_wait(void *handle, size_t page_count,
	unsigned int align);

void codec_mm_compaction_init(void *data);

void codec_mm_compaction_start(int rounds);
void codec_mm_compaction_stop(void);
int codec_mm_compact_trylock(void);
void codec_mm_compact_lock(void);
void codec_mm_compact_unlock(void);

/* Called by pagemap thread when it finishes, so compaction can start */
void codec_mm_pagemap_done_signal(void);

#endif //_CODEC_MM_COMPACT_H

