/* Generate tone-curve samples and hardware LUTs. */
#include "tone_curve.h"
#include "numeric_tone.h"
#include "tone_mapping_math.h"
#include "fixed_arithmetic.h"
#include "fixed_transcendentals.h"

void dovi_curve_sample_primary_lut(void *control, void *parameters)
{
    unsigned long sample_input;
    unsigned int *samples;
    int linear_input;
    unsigned int mapped_sample;
    sample_input = 0;
    samples = *(unsigned int **)((long)control + 6000);
    do {
        linear_input = dovi_HlgOotf_I(sample_input & 0xffffffff, parameters);
        mapped_sample = dovi_TMIcore(parameters, linear_input << 10);
        sample_input = sample_input + 0x80;
        *samples = mapped_sample;
        samples = samples + 1;
    } while (sample_input != 0x10000);
}

void dovi_curve_sample_identity_lut(void *control, void *parameters)
{
    int *samples;
    int *sample_pair;
    int linear_sample;
    int sample;
    unsigned long sample_input;
    long byte_offset;
    samples = *(int **)((long)control + 6000);
    if (*(int *)((long)control + 0xd8) == 1) {
        linear_sample = 0;
        sample_input = 0;
        do {
            sample = linear_sample;
            if (*(int *)((long)control + 0x1668) != 0) {
                sample = dovi_HlgOotf_I(sample_input & 0xffffffff, parameters);
                sample = sample << 10;
            }
            sample_input = sample_input + 0x80;
            linear_sample = linear_sample + 0x20100;
            *samples = sample;
            samples = samples + 1;
        } while (sample_input != 0x10000);
    } else {
        linear_sample = 0;
        byte_offset = 0;
        do {
            sample_pair = (int *)((long)samples + byte_offset);
            byte_offset = byte_offset + 8;
            *sample_pair = linear_sample;
            sample_pair[1] = linear_sample + 0x20100;
            linear_sample = linear_sample + 0x40200;
        } while (byte_offset != 0x800);
    }
}

void dovi_curve_pack_luts(void *control, void *output)
{
    unsigned short sample;
    unsigned int rounded;
    long source_offset;
    long output_offset;
    long sample_index;

    sample_index = 0;
    do {
        source_offset = sample_index * 4;
        rounded = *(int *)(*(long *)((long)control + 6000) + source_offset) * 4 + 0x2000;
        sample = (unsigned short)(rounded >> 0x10);
        output_offset = sample_index * 2;
        if (0xfffffff < (int)(rounded & 0xffff0000)) {
            sample = 0xfff;
        }
        sample_index = sample_index + 1;
        *(unsigned short *)((long)output + output_offset + 4) = sample;
        *(int *)((long)control + source_offset + 0x220) =
            (*(int *)(*(long *)((long)control + 6000) + source_offset) + 0x4000) >> 0xb;
    } while (sample_index != 0x200);
}
