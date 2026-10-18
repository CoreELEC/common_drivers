/* Output color-space and transfer maps. */
#ifndef DOVI_DM_OUTPUT_MAP_H
#define DOVI_DM_OUTPUT_MAP_H

int dovi_dm_CommitSigEnv2KsOMap(unsigned int signal_index, long config, long context);
void dovi_dm_UpdateKsOMapTmm(unsigned int signal_index, long config, long context);
unsigned long dovi_dm_Commit2Dm3KsOMapBypass(unsigned int bit_depth, long output_map);
#endif
