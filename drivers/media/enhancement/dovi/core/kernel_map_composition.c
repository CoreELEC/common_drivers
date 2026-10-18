/* Concatenate primary inverse mapping with tone and saturation maps. */
#include "kernel_map_composition.h"
#include "tone_parameters.h"
#include "tone_mapping_math.h"
#include "fixed_arithmetic.h"
#include "dm_types.h"

unsigned long dovi_ConcatenatePriBwdMapping(void *control, void *mapping, void *inverse,
                                            void *context)
{
    long control_address = (long)control;
    long mapping_address = (long)mapping;
    short *inverse_map = inverse;
    long context_address = (long)context;
    _Bool more_entries;
    unsigned int index_or_scaled_value;
    int *luma_values;
    short *second_saturation_lut;
    short *first_saturation_lut;
    short *luma_knots;
    long mapping_entry_address;
    unsigned int rounded_tone_high_word;
    short clamped_saturation;
    unsigned int saturation_coordinate;
    int tone_value_or_upper_index;
    unsigned long index_or_quotient;
    short *saturation_denominator;
    short knot_or_clamped_value;
    unsigned long lower_value_offset;
    long entry_index;
    unsigned long lower_knot_offset;

    dovi_CopyDataFromTcCtrlToTC((void *)(control_address + 0x1650), control_address);
    if (-1 < *inverse_map) {
        luma_values = (int *)(context_address + 0x1d7c0);
        second_saturation_lut = (short *)(context_address + 0x18ba0);
        first_saturation_lut = (short *)(context_address + 0x1839c);
        luma_knots = (short *)(context_address + 0x17f9c);
        entry_index = 0;
        do {
            if (entry_index == 0x200) {
                /* The mapping has at most 512 entries. */
                __asm__ volatile("brk #0x5512");
                __builtin_unreachable();
            }
            knot_or_clamped_value = *(short *)(mapping_address + entry_index * 2 + 4);
            saturation_coordinate = *(unsigned int *)(context_address + entry_index * 4 + 0x1cfc0);
            if (knot_or_clamped_value < *luma_knots) {
                tone_value_or_upper_index = *luma_values;
            scale_endpoint_luma:
                tone_value_or_upper_index = tone_value_or_upper_index << 3;
            } else {
                index_or_quotient = 0xffffffff;
                do {
                    tone_value_or_upper_index = (int)index_or_quotient + 2;
                    if (*(int *)(context_address + 0x1dfc0) < tone_value_or_upper_index) {
                        tone_value_or_upper_index = luma_values[*(int *)(context_address + 0x1dfc0)];
                        goto scale_endpoint_luma;
                    }
                    index_or_scaled_value = (int)index_or_quotient + 1;
                    index_or_quotient = (unsigned long)index_or_scaled_value;
                } while ((int)luma_knots[tone_value_or_upper_index] <= (int)knot_or_clamped_value);
                lower_value_offset = (-(unsigned long)(index_or_scaled_value >> 0x1f) & 0xfffffffc00000000) | index_or_quotient << 2;
                lower_knot_offset = (-(unsigned long)(index_or_scaled_value >> 0x1f) & 0xfffffffe00000000) | index_or_quotient << 1;
                index_or_scaled_value = luma_values[tone_value_or_upper_index] - *(int *)((long)luma_values + lower_value_offset);
                index_or_quotient = dovi_sdiv((-(unsigned long)(index_or_scaled_value >> 0x1f) & 0xfff8000000000000) |
                                       (unsigned long)index_or_scaled_value << 0x13,
                                   (int)luma_knots[tone_value_or_upper_index] - (int)*(short *)((long)luma_knots + lower_knot_offset));
                tone_value_or_upper_index =
                    (((int)knot_or_clamped_value - (int)*(short *)((long)luma_knots + lower_knot_offset)) * (int)index_or_quotient >> 0x10) +
                    *(int *)((long)luma_values + lower_value_offset) * 8;
            }
            inverse_map[entry_index + 2] = (short)tone_value_or_upper_index;
            tone_value_or_upper_index = dovi_SMIcore((void *)(control_address + 0x1650), tone_value_or_upper_index << 0xe, saturation_coordinate << 0x11, 1);
            index_or_scaled_value = tone_value_or_upper_index * 4 + 8;
            mapping_entry_address = mapping_address + entry_index * 2;
            rounded_tone_high_word = index_or_scaled_value & 0xffff0000;
            knot_or_clamped_value = (short)(index_or_scaled_value >> 0x10);
            if (0x2000ffff < (int)rounded_tone_high_word) {
                knot_or_clamped_value = 0x2000;
            }
            if ((int)rounded_tone_high_word < -0xffff) {
                knot_or_clamped_value = 0;
            }
            inverse_map[entry_index + 0x404] = knot_or_clamped_value;
            saturation_denominator = first_saturation_lut;
            if (0 < (int)(saturation_coordinate * 8)) {
                knot_or_clamped_value = *(short *)(context_address + 0x17f98);
                if ((int)((unsigned long)saturation_coordinate & 0x1fffffff) < (int)knot_or_clamped_value) {
                    saturation_denominator = first_saturation_lut + ((unsigned long)saturation_coordinate & 0x1fffffff);
                } else {
                    saturation_denominator = first_saturation_lut + (int)knot_or_clamped_value;
                }
            }
            index_or_quotient = dovi_sdiv((long)*(short *)(mapping_entry_address + 0x404) << 0x1c, (int)*saturation_denominator);
            knot_or_clamped_value = (short)(index_or_quotient >> 0x10);
            if (0x2000ffff < (int)index_or_quotient) {
                knot_or_clamped_value = 0x2000;
            }
            clamped_saturation = 0;
            if (-1 < (int)index_or_quotient) {
                clamped_saturation = knot_or_clamped_value;
            }
            inverse_map[entry_index + 0x202] = clamped_saturation;
            saturation_denominator = second_saturation_lut;
            if (0 < (int)(saturation_coordinate * 8)) {
                knot_or_clamped_value = *(short *)(context_address + 0x17f98);
                if ((int)(saturation_coordinate & 0x1fffffff) < (int)knot_or_clamped_value) {
                    saturation_denominator = second_saturation_lut + (saturation_coordinate & 0x1fffffff);
                } else {
                    saturation_denominator = second_saturation_lut + (int)knot_or_clamped_value;
                }
            }
            index_or_quotient = dovi_sdiv((long)*(short *)(mapping_entry_address + 0xc08) << 0x1c, (int)*saturation_denominator);
            knot_or_clamped_value = (short)(index_or_quotient >> 0x10);
            if (0x2000ffff < (int)index_or_quotient) {
                knot_or_clamped_value = 0x2000;
            }
            clamped_saturation = 0;
            if (-1 < (int)index_or_quotient) {
                clamped_saturation = knot_or_clamped_value;
            }
            inverse_map[entry_index + 0x604] = clamped_saturation;
            more_entries = entry_index < *inverse_map;
            entry_index = entry_index + 1;
        } while (more_entries);
    }
    return 1;
}
