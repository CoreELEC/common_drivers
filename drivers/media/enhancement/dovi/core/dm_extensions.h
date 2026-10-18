/* Sort, replace, remove and append display-metadata extension records. */
#ifndef DOVI_DM_EXTENSIONS_H
#define DOVI_DM_EXTENSIONS_H

#include "parser_contract.h"

#define DOVI_DM_EXTENSION_STRIDE 0x108u
#define DOVI_DM_EXTENSION_COUNT_OFFSET 0x4au
#define DOVI_DM_EXTENSION_RECORDS_OFFSET 0x4cu

/*
 * DM count is a byte at +0x4a; records start at +0x4c with stride 0x108.
 * Each record contains a four-byte value at +0, byte type at +4 and 256
 * payload bytes at +6. Copies leave padding +5, +0x106 and +0x107 untouched.
 * Buffers must be valid, disjoint and stable. Removal masks span 256 bytes.
 * Scratch needs count+1 accessible records: the original reads a sentinel
 * type before checking the count, even for an empty list. Scratch writes
 * preserve padding and all records beyond the number sorted.
 *
 * update/remove sort types 1..255, omitting type 0. New records replace all
 * existing records of their type; duplicate new records retain their order.
 * update treats count as signed32 for its input-loop enable check, while
 * its other comparisons preserve the original unsigned value. Output count
 * and output write indices wrap at 256. Append writes linearly, requiring
 * space for old count + new count records even though its count wraps.
 * No buffer bounds checks or fixes are introduced.
 */
void dovi_update_dm_extensions(void *dm, const void *blocks, unsigned int count, void *scratch);
void dovi_remove_dm_extensions(void *dm, const unsigned char *remove, void *scratch);
#if PARSER_DECODER_EXTENSION_API
void dovi_append_dm_extensions(void *dm, const void *blocks, unsigned char count);
#endif

#endif
