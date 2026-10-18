#ifndef DOVI_TONE_PARAMETERS_H
#define DOVI_TONE_PARAMETERS_H
#include "tone_curve.h"

/* All input objects and output pointers are valid. Output stores and reloads
 * retain the order of the STB 2.4 machine ABI, including output aliases. */
void calc_mapped_params(int source_max, struct tone_curve_source *source,
                        struct tone_curve_display *display, int *target_min, int *target_mid,
                        int *target_max, int *source_mid, int *slope, int *source_target_ratio,
                        int *screen_ratio);
void calc_curve_params(int source_min, int source_mid, int source_max, int target_min,
                       int target_mid, int target_max, int slope,
                       struct tone_curve_display *display, struct tone_curve_pair *param1,
                       struct tone_curve_pair *param2, struct tone_curve_pair *param3,
                       int *slope_over_rolloff);
void dovi_CalcMappedParams2pxx(int crush, int mid, int clip, struct tone_curve_display *display,
                               int source_min_pq, int source_max_pq, int source_diagonal,
                               int *target_min, int *target_mid, int *target_max, int *source_mid,
                               int *slope);
#endif
