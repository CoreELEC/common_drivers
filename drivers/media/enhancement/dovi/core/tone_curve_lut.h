#ifndef DOVI_TONE_CURVE_LUT_H
#define DOVI_TONE_CURVE_LUT_H
#include "tone_curve.h"
extern const int tone_curve_xlog_table[4096];
long long fx_pow2_taylor_int(int value, int scale_in, int scale_out);
/* x_min/x_max are unused. Positive lut_size <=4096
 * writes N=ceil(4096 / floor(4096/lut_size)) samples. A null lut4k needs N
 * work elements. A nonnull lut4k needs max(515,N+1) work elements and 4096
 * output elements: samples begin at work[1], then the fixed interpolation
 * reads work[0..514]. Step 8 (lut_size 456..512) gives 512 samples.
 * Other steps retain that fixed interpolation, including untouched work cells.
 * Sizes <=0 or >4096 do not describe a safe finite call. */
void generate_tone_curve_lut(int x_min, int x_max, const struct tone_curve_pair *param1,
                             const struct tone_curve_pair *param2,
                             const struct tone_curve_pair *param3, int slope_over_rolloff,
                             const struct tone_curve_display *display, int trim_slope,
                             int trim_power, int trim_offset, short *lut515, short *lut4k,
                             short lut_size, short clamp_output);
#endif
