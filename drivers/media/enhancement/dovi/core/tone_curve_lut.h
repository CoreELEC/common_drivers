#ifndef DOVI_TONE_CURVE_LUT_H
#define DOVI_TONE_CURVE_LUT_H
void dovi_curve_sample_primary_lut(void *control, void *parameters);
void dovi_curve_sample_identity_lut(void *control, void *parameters);
void dovi_curve_pack_luts(void *control, void *output);
#endif
