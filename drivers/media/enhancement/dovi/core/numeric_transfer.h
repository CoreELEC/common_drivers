#ifndef DOVI_NUMERIC_TRANSFER_H
#define DOVI_NUMERIC_TRANSFER_H

void calc_ta_tb(int gamma, unsigned int maximum, unsigned int minimum, int *ta, int *tb);
int dovi_applyGamma(int linear, int inverse_gamma);
int dovi_computeTgtEotfLut(void *lut, unsigned int maximum, unsigned int minimum,
                           unsigned int gamma, unsigned int eotf);
unsigned int dovi_L2PQ_loglut(unsigned int linear);
int dovi_l2eotf_lerp(unsigned int linear, const void *lut, unsigned int minimum,
                     unsigned int maximum);
void dovi_GenerateL2GLut(unsigned int gamma, void *slopes, void *lut16, void *positions,
                         void *scratchx, void *scratchy);

#endif
