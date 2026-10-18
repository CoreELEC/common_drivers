#ifndef DOVI_NUMERIC_HELPERS_H
#define DOVI_NUMERIC_HELPERS_H

unsigned int dovi_ConvertLvl2LvlBit(unsigned int level);
unsigned int dovi_SetLvl(unsigned int bits, unsigned int level);
unsigned int dovi_ClearLvl(unsigned int bits, unsigned int level);
unsigned int dovi_HaveLvlSet(unsigned int bits, unsigned int level);
void dovi_SortAscend(void *values, void *indices, unsigned int last);
void dovi_GetRange(unsigned int bits, unsigned int range, void *minimum, void *extent);
void dovi_GetCvmBypassNormParam(unsigned int raw_bits, unsigned int range, void *offset,
                                void *scale);
void dovi_GetCvmBypassDnParam(unsigned int depth, unsigned int full, void *offset, void *factor);
void dovi_CommitKsUs(const void *format, unsigned int flags, void *output);
void dovi_CommitKsDs(const void *format, void *output);
unsigned long long dovi_Interp1DFxp(const void *table, unsigned int last, unsigned int shift,
                                    unsigned int mask, unsigned int value);
unsigned int dovi_Interp1DForInverseLUTFxp(const void *table, const void *indices,
                                           unsigned int last, unsigned int value);
unsigned int dovi_InvLUT1DNorm(const void *table, unsigned int last, unsigned int value);
unsigned int dovi_InvLUT1D(unsigned int value, unsigned int low, unsigned int high,
                           const void *table, unsigned int last);
void *dovi_GetDisplay(unsigned int index);
const void *dovi_GetPrimariesPtr(unsigned int index);

#endif
