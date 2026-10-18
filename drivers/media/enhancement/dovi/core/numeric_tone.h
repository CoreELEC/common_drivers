#ifndef DOVI_NUMERIC_TONE_H
#define DOVI_NUMERIC_TONE_H
#include "numeric_common.h"
unsigned long long dovi_TmIFwd(unsigned int value, const void *context);
unsigned long long dovi_TmSFwd(unsigned int value, const void *context);
unsigned int dovi_SmIFwd(unsigned int reference, unsigned int value, const void *context);
unsigned int dovi_SmSFwd(unsigned int value, const void *context);
unsigned long long dovi_TmIBwd(unsigned int value, const void *context);
unsigned int dovi_TmSBwd(unsigned int value, const void *context);
unsigned int dovi_SmIBwd(unsigned int reference, unsigned int value, const void *context);
unsigned int dovi_SmSBwd(unsigned int value, const void *context);
unsigned int dovi_HlgOotf_I(unsigned int value, const void *context);
#endif
