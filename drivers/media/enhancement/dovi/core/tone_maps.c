/* Generate auxiliary tone and saturation LUTs around the primary curve. */
#include "tone_maps.h"
#include "tone_curve_lut.h"
#include "numeric_tone.h"
#include "tone_mapping_math.h"
#include "fixed_arithmetic.h"
#include "fixed_transcendentals.h"
#include "dm_types.h"

void dovi_curve_build_luts(void *control, void *output, void *parameters)
{
    unsigned short clamped_sample;
    unsigned short scaled_sample;
    unsigned int rounded_sample;
    int sample_work;
    int square_root;
    int curve_sample;
    unsigned long root_sample;
    long root_cursor;
    int *root_output;
    long lut_cursor;
    long sample_cursor;
    int *root_input;

    if (*(unsigned int *)((long)control + 0xe4) < 2) {
        if (*(unsigned int *)((long)control + 0xe4) == 1) {
            dovi_curve_sample_primary_lut(control, parameters);
            root_input = (int *)((long)control + 0x1778);
            curve_sample = 0;
            sample_work = 0;
            sample_cursor = 0;
            *(unsigned int *)((long)control + 0x1778) = 0;
            root_output = root_input;
            do {
                if (curve_sample < 1) {
                    root_sample = 0;
                } else {
                    square_root = dovi_ssqrt_s(sample_work);
                    root_sample = (unsigned long)(unsigned int)(square_root >> 1);
                }
                if (root_output != (int *)0x0) {
                    *root_output = (int)root_sample;
                }
                dovi_sdiv((-(root_sample >> 0x1f) & 0xff80000000000000) | root_sample << 0x17,
                          *(unsigned int *)((long)control + 0x1764));
                sample_work = sample_work + 0x200000;
                curve_sample = curve_sample + 0x40000;
                root_output = root_output + 1;
                rounded_sample = (int)((unsigned long)((long)*(int *)((long)control + 0x16cc) *
                                              (long)(int)root_sample) >>
                              0x1b) +
                        0x4000800;
                scaled_sample = (unsigned short)(rounded_sample >> 0xe);
                if (0x8003fff < (int)rounded_sample) {
                    scaled_sample = 0x2000;
                }
                clamped_sample = 0;
                if (-1 < (int)rounded_sample) {
                    clamped_sample = scaled_sample;
                }
                *(unsigned short *)((long)output + sample_cursor + 0x404) = clamped_sample;
                sample_cursor = sample_cursor + 2;
            } while (sample_cursor != 0x400);
            sample_work = 0;
            sample_cursor = 0;
            lut_cursor = *(long *)((long)control + 6000);
            do {
                curve_sample =
                    dovi_SMIcore(parameters, *(unsigned int *)(lut_cursor + sample_cursor * 4), sample_work, 0);
                rounded_sample = curve_sample + 2;
                sample_work = sample_work + 0x20000;
                scaled_sample = (unsigned short)(rounded_sample >> 0xe);
                if (0x8003fff < (int)rounded_sample) {
                    scaled_sample = 0x2000;
                }
                clamped_sample = 0;
                if (-1 < (int)rounded_sample) {
                    clamped_sample = scaled_sample;
                }
                *(unsigned short *)((long)output + sample_cursor * 2 + 0x808) = clamped_sample;
                sample_cursor = sample_cursor + 1;
            } while (sample_cursor != 0x200);
            sample_cursor = 0;
            do {
                sample_work = dovi_SMScore(parameters, root_input, 0, 1, 1);
                rounded_sample = sample_work + 0x800;
                root_input = root_input + 1;
                scaled_sample = (unsigned short)(rounded_sample >> 0xe);
                if (0x8003fff < (int)rounded_sample) {
                    scaled_sample = 0x2000;
                }
                clamped_sample = 0;
                if (-1 < (int)rounded_sample) {
                    clamped_sample = scaled_sample;
                }
                *(unsigned short *)((long)output + sample_cursor + 0xc08) = clamped_sample;
                sample_cursor = sample_cursor + 2;
            } while (sample_cursor != 0x400);
        } else {
            dovi_curve_sample_primary_lut(control, parameters);
            sample_work = 0;
            root_cursor = (long)control + 0x1778;
            lut_cursor = 0;
            *(unsigned int *)((long)control + 0x1778) = 0;
            sample_cursor = root_cursor;
            do {
                curve_sample = dovi_TMScore(parameters, sample_work, (void *)sample_cursor, 1);
                sample_work = sample_work + 0x40000;
                sample_cursor = sample_cursor + 4;
                *(short *)((long)output + lut_cursor + 0x404) = (short)((curve_sample + 0x800U) >> 0xe);
                lut_cursor = lut_cursor + 2;
            } while (lut_cursor != 0x400);
            sample_work = 0;
            sample_cursor = 0;
            lut_cursor = *(long *)((long)control + 6000);
            do {
                curve_sample =
                    dovi_SMIcore(parameters, *(unsigned int *)(lut_cursor + sample_cursor * 4), sample_work, 0);
                rounded_sample = curve_sample + 2;
                sample_work = sample_work + 0x20000;
                scaled_sample = (unsigned short)(rounded_sample >> 0xe);
                if (0x8003fff < (int)rounded_sample) {
                    scaled_sample = 0x2000;
                }
                clamped_sample = 0;
                if (-1 < (int)rounded_sample) {
                    clamped_sample = scaled_sample;
                }
                *(unsigned short *)((long)output + sample_cursor * 2 + 0x808) = clamped_sample;
                sample_cursor = sample_cursor + 1;
            } while (sample_cursor != 0x200);
            sample_cursor = 0;
            do {
                sample_work = dovi_SMScore(parameters, (void *)root_cursor, 0, 0, 1);
                rounded_sample = sample_work + 0x800;
                root_cursor = root_cursor + 4;
                scaled_sample = (unsigned short)(rounded_sample >> 0xe);
                if (0x8003fff < (int)rounded_sample) {
                    scaled_sample = 0x2000;
                }
                clamped_sample = 0;
                if (-1 < (int)rounded_sample) {
                    clamped_sample = scaled_sample;
                }
                *(unsigned short *)((long)output + sample_cursor + 0xc08) = clamped_sample;
                sample_cursor = sample_cursor + 2;
            } while (sample_cursor != 0x400);
        }
    } else {
        dovi_curve_sample_identity_lut(control, parameters);
        sample_cursor = 0;
        do {
            lut_cursor = sample_cursor + 4;
            *(unsigned int *)((long)output + sample_cursor + 0x404) = 0x10001000;
            sample_cursor = lut_cursor;
        } while (lut_cursor != 0x400);
        if (*(int *)(*(long *)((long)control + 0x1b0) + 0x20) == 0) {
            sample_cursor = 0;
            do {
                lut_cursor = sample_cursor + 4;
                *(unsigned int *)((long)output + sample_cursor + 0x808) = 0x10001000;
                sample_cursor = lut_cursor;
            } while (lut_cursor != 0x400);
        } else {
            sample_work = 0;
            sample_cursor = 0;
            lut_cursor = *(long *)((long)control + 6000);
            do {
                curve_sample =
                    dovi_SMIcore(parameters, *(unsigned int *)(lut_cursor + sample_cursor * 4), sample_work, 0);
                rounded_sample = curve_sample + 2;
                sample_work = sample_work + 0x20000;
                scaled_sample = (unsigned short)(rounded_sample >> 0xe);
                if (0x8003fff < (int)rounded_sample) {
                    scaled_sample = 0x2000;
                }
                clamped_sample = 0;
                if (-1 < (int)rounded_sample) {
                    clamped_sample = scaled_sample;
                }
                *(unsigned short *)((long)output + sample_cursor * 2 + 0x808) = clamped_sample;
                sample_cursor = sample_cursor + 1;
            } while (sample_cursor != 0x200);
        }
        sample_cursor = 0;
        do {
            lut_cursor = sample_cursor + 4;
            *(unsigned int *)((long)output + sample_cursor + 0xc08) = 0x10001000;
            sample_cursor = lut_cursor;
        } while (lut_cursor != 0x400);
    }
}
