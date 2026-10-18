#ifndef DOVI_NUMERIC_TRANSFER_H
#define DOVI_NUMERIC_TRANSFER_H

#include "numeric_common.h"

void dovi_Cct2Xy(unsigned long long cct, void *out);
void dovi_Xy2Xyz(const void *input, void *output);
void dovi_Xyz2IctcpDm(const void *input, void *output);
void dovi_RgbLn2Ictcp(unsigned int red, unsigned int green, unsigned int blue, const void *matrix,
                      unsigned long long scale, const void *matrix2, unsigned int scale2,
                      void *output);
unsigned long long dovi_GetIntendedWpCct(unsigned int source, unsigned int target,
                                         unsigned int index);
void dovi_GetGammaABG(unsigned int gamma, unsigned long long maximum, unsigned long long minimum,
                      void *alpha, void *beta, void *outgamma);
void dovi_GenerateL2GLut(unsigned int gamma, void *slopes, void *lut16, void *positions,
                         void *scratchx, void *scratchy);

extern unsigned int dovi_Xyz2IctcpDm_m33Xyz2Lms[9], dovi_Xyz2IctcpDm_v3_0, dovi_Xyz2IctcpDm_v3_1;

#endif
