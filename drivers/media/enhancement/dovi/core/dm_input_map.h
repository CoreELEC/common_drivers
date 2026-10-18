/* Input color-space and transfer maps. */
#ifndef DOVI_DM_INPUT_MAP_H
#define DOVI_DM_INPUT_MAP_H

_Bool dovi_dm_CanCclrBypassCsc(int color_space, int range, int bypass);
void dovi_dm_CommitMds2KsIMap(int signal_index, unsigned char *metadata, long runtime_signal,
                              long csc_control, unsigned char *input_map);
int dovi_dm_CommitIptRgb2LmsM33(int signal_index, int color_space, long csc_control, short *matrix);
void dovi_dm_CommitIpt2KsIMap(int signal_index, long csc_control, int color_space,
                              unsigned int bit_depth, int range, int bypass,
                              unsigned char *input_map);
unsigned long dovi_dm_CommitSigEnv2KsIMap(int *signal, long csc_control, unsigned char *input_map);
unsigned long dovi_dm_Commit2Dm3KsIMapBypass(unsigned int bit_depth, unsigned char *input_map);
#endif
