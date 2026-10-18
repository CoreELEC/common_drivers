/* Display-management trim processing. */
#ifndef DOVI_DM_TRIM_H
#define DOVI_DM_TRIM_H

void dovi_ClampLevel2(int *level2);
int dovi_dm_CommitTrim(unsigned short target_max_pq, void *extensions, void *dm,
                       void *tone_control);

/* Chroma distance (dm_trim_chroma.c). */
unsigned int dovi_dm_FindChromaDist(const void *primaries, unsigned int luminance, int target_pq);
extern unsigned char chroma_distance_state[116];
extern const int chroma_xyz_to_lms[3][3];
extern const short chroma_lms_to_ictcp[3][3];
#endif
