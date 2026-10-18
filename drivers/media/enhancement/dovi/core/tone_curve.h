#ifndef DOVI_TONE_CURVE_H
#define DOVI_TONE_CURVE_H
#include "dm_types.h"
#include "tone_parameters.h"
#include "tone_curve_lut.h"
#include "tone_maps.h"

#define DOVI_CURVE_MIN_LINEAR_LUMINANCE 0x140000UL

int dovi_CreateToneCurve(void *control, void *output, unsigned int mode, void *curve,
                                  void *context);

#endif
