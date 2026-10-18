/* Signal environments and configuration defaults. */
#ifndef DOVI_DM_CONFIGURATION_H
#define DOVI_DM_CONFIGURATION_H

void dovi_dm_InitSrcSigEnvPri(int *signal_words, int signal_index);
void dovi_dm_InitSrcSigEnvSec(int *signal_words, int signal_index);
void dovi_dm_InitAlpFrmFmt(unsigned int *frame_words, unsigned int signal_index);
void dovi_dm_InitAlpSigEnv(unsigned long *signal_words);
void dovi_dm_InitTgtSigEnv(void *signal);
unsigned long dovi_dm_GetSigEnv(void *output, unsigned int signal_index, long context);
long dovi_dm_GetSigEnvPtr(unsigned int signal_index, long context);
int dovi_dm_GetTmCtrl(void *output, long context);
void dovi_dm_InitGdCtrl(unsigned long *control_words);
unsigned long dovi_dm_GetGdCtrl(void *output, long context);
void dovi_dm_InitDmCtrl(unsigned long *control_words);
int dovi_dm_GetDmCtrl(void *output, long context);
unsigned long *dovi_dm_InitDmCfg(unsigned long unused, void *memory_manager,
                                 unsigned long *config_words);
#endif
