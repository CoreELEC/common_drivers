/* Display-management trim processing. */
#ifndef DOVI_DM_TRIM_H
#define DOVI_DM_TRIM_H

void dovi_dm_FindTrimIdxUsed(int *trim_header, long metadata, long signal, long context,
                             unsigned int *indices, int *index_count);
void dovi_dm_SetDefaultL8Trim(long tone_control, long trim);
void dovi_dm_SetDefaultL2Trim(long tone_control, long trim);
unsigned int dovi_dm_GetTMaxFromTrim(int *trim_header, int trim_index, long metadata);

/* Chroma distance (dm_trim_chroma.c). */
unsigned int dovi_dm_FindChromaDist(const void *primaries, unsigned int luminance, int target_pq);
#endif
