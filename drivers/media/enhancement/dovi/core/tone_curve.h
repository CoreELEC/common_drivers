#ifndef DOVI_TONE_CURVE_H
#define DOVI_TONE_CURVE_H

struct tone_curve_source {
    int crush, mid, clip, min_pq, max_pq, key_weight;
    int trim_slope, trim_offset, trim_power, chroma_weight;
    int saturation_gain, diagonal_inches, intensity_weight, chroma_vector_weight;
};

struct tone_curve_display {
    int min_pq, max_pq, min_bias, mid_bias, max_bias;
    int contrast, brightness, diagonal_inches, bright_preserve, rolloff, rolloff_inverse;
    unsigned short mode;
};

struct tone_curve_pair {
    int value;
    unsigned short norm;
};

/* Three-point tone-curve generation. Source fields are clamped in place.
 * With ratio NULL all four LUTs are filled; nonnull ratio skips the three
 * auxiliary maps. Objects need complete backing; LUT sizes follow the finite
 * domain documented by generate_tone_curve_lut. */
void create_tone_curve_3pxx(struct tone_curve_source *source, struct tone_curve_display *display,
                            struct tone_curve_pair *param1, struct tone_curve_pair *param2,
                            struct tone_curve_pair *param3, int *slope_over_rolloff, short *tone_i,
                            short *tone_s, short *saturation_i, short *saturation_s, int *ratio,
                            short lut_size, short clamp_output);
/* Work storage has 515 shorts. A nonnull final tone output has 4096 shorts;
 * otherwise the work buffer receives the padded 515-element curve. Mode 3
 * returns without changing outputs. Other modes retain caller-supplied curve
 * parameters unless mode 0 or 1 recomputes them. Buffers may overlap. */
void dovi_CreateToneCurve2pxx(struct tone_curve_source *source, struct tone_curve_display *display,
                              struct tone_curve_pair *param1, struct tone_curve_pair *param2,
                              struct tone_curve_pair *param3, int *slope_over_rolloff,
                              short *tone_with_padding, short *tone);
/* Compare/copy covers the first 160 bytes of the 168-byte TcCtrl object. */
int dovi_CreateToneCurve(const void *tone_curve, void *kernel_tmap, void *used_curve);
int create_tone_curve_changed(const void *tone_curve, void *kernel_tmap, void *used_curve);
#endif
