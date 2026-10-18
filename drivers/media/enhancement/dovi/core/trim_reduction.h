#ifndef DOVI_TRIM_REDUCTION_H
#define DOVI_TRIM_REDUCTION_H
#include "dm_trim.h"
/* Packed control metadata: count at byte 70, records at 71 with stride 261.
 * reduce_trims requires at most sixteen level-2 and sixteen level-8 records,
 * matching the fixed local arrays. target_pointer_address points to
 * an actual 64-bit pointer to the 88-byte target descriptor, not to a context.
 * Direct sorting requires caller arrays large enough for all level-2 records.
 */
int reduce_trims(const void *target_pointer_address, void *packed_metadata);
int sort_l2_by_tmax_lum(const void *packed_metadata, int *indices, int *max_pq);
extern const int trim_display_table[7][4];
extern const int trim_primaries_table[20][8];
#endif
