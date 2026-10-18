/* Transfer-function selection and fixed-point de-gamma. */
#ifndef DOVI_DM_TRANSFER_H
#define DOVI_DM_TRANSFER_H

/* Gain, offset, exponent and sample use their low16 bits; overflow_scale uses32. */
unsigned long dovi_dm_DeGamma(unsigned int gain, unsigned int offset, unsigned int exponent,
                              unsigned int overflow_scale, unsigned int sample);
int dovi_dm_GetTcLutDir(const void *context);
void dovi_dm_GetEotfFromKs(unsigned int signal_index, long context, long signal);
int dovi_dm_GetTcLutDirIdx(unsigned int signal_index, long context);
int dovi_dm_GetTcLutDirPri(long context);
#endif
