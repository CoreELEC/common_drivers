/* Trim interpolation brackets and inverse lookup tables. */
#include "dm_bindings.h"
#include "dm_layout.h"

void dovi_dm_FindInterpIdx(int *coordinates, int last_index, int target, int *lower_index,
                           int *upper_index)
{
    int next;
    int lower;

    *upper_index = 0;
    *lower_index = 0;
    if ((last_index != 0) && (*coordinates <= target)) {
        next = *upper_index;
        do {
            lower = next;
            next = lower + 1;
            *upper_index = next;
            if (last_index < next)
                break;
        } while (coordinates[next] <= target);
        *lower_index = lower;
        if (last_index < *upper_index) {
            *upper_index = lower;
        }
    }
    return;
}

unsigned int dovi_dm_BuildInterpInfo(long config, long metadata, int *trim_header, int *coordinates,
                                     long indices, int last_index, int target,
                                     unsigned int *weight_q12, long default_trim, long *lower_trim,
                                     long *upper_trim)
{
    int trim_index;
    unsigned int coordinate_delta;
    unsigned int weight;
    unsigned long quotient;
    int lower_index;
    long selected_trim;
    unsigned int uses_default;
    int upper_index;

    dovi_SortAscend((void *)(coordinates), (void *)(indices), (unsigned int)(last_index));
    if ((last_index == 0) || (target < *coordinates)) {
        lower_index = 0;
        upper_index = 0;
    } else {
        trim_index = 0;
        do {
            lower_index = trim_index;
            trim_index = lower_index + 1;
            upper_index = lower_index;
            if (last_index < trim_index)
                break;
            upper_index = trim_index;
        } while (coordinates[trim_index] <= target);
    }
    if ((*(int *)(indices + (long)lower_index * 4) == trim_header[2]) ||
        (*(int *)(indices + (long)upper_index * 4) == trim_header[2])) {
        if (upper_index == lower_index) {
            uses_default = 1;
        } else if (*trim_header == 8) {
            dovi_dm_SetDefaultL8Trim((long)(config + 0x708), (long)(default_trim));
            uses_default = 1;
        } else {
            *(short *)(default_trim + 2) = ((short)*(unsigned int *)(config + 0x78c) -
                                            (short)*(unsigned int *)(config + 0x7e0)) *
                                               2 +
                                           -0x800;
            *(short *)(default_trim + 4) = ((short)*(unsigned int *)(config + 0x790) -
                                            (short)*(unsigned int *)(config + 0x7e4)) *
                                               2 +
                                           -0x800;
            *(short *)(default_trim + 6) = ((short)*(unsigned int *)(config + 0x794) -
                                            (short)*(unsigned int *)(config + 0x7e8)) *
                                               2 +
                                           -0x800;
            *(short *)(default_trim + 8) =
                (short)*(unsigned int *)(config + 0x798) - (short)*(unsigned int *)(config + 0x7ec);
            *(short *)(default_trim + 10) =
                (short)*(unsigned int *)(config + 0x79c) - (short)*(unsigned int *)(config + 0x7f0);
            uses_default = 1;
            *(unsigned short *)(default_trim + 0xc) =
                (unsigned short)((unsigned int)(*(int *)(config + 0x7a0) -
                                                *(int *)(config + 0x7f4)) >>
                                 1) &
                0x7fff;
        }
    } else {
        uses_default = 0;
    }
    trim_index = *(int *)(indices + (long)lower_index * 4);
    selected_trim = default_trim;
    if (trim_index != trim_header[2]) {
        if (*trim_header == 8) {
            selected_trim = metadata + (long)trim_index * 0x2a + 0x11c;
        } else {
            selected_trim = metadata + (long)trim_index * 0xe + 0x90;
        }
    }
    *lower_trim = selected_trim;
    trim_index = *(int *)(indices + (long)upper_index * 4);
    if (trim_index != trim_header[2]) {
        if (*trim_header == 8) {
            default_trim = metadata + (long)trim_index * 0x2a + 0x11c;
        } else {
            default_trim = metadata + (long)trim_index * 0xe + 0x90;
        }
    }
    *upper_trim = default_trim;
    if (lower_index == upper_index) {
        weight = 0;
    } else {
        coordinate_delta = target - coordinates[lower_index];
        quotient = dovi_sdiv(
            (unsigned long long)((-(unsigned long)(coordinate_delta >> 0x1f) & 0xfffff00000000000) |
                                 (unsigned long)coordinate_delta << 0xc),
            (unsigned int)(coordinates[upper_index] - coordinates[lower_index]));
        weight = (unsigned int)quotient;
    }
    *weight_q12 = weight;
    return uses_default;
}

void dovi_dm_MakeTheInverseLutUnique(long input_lut, unsigned int *output_lut, short *inverse_lut,
                                     int *last_index)
{
    short previous_value;
    int output_index;
    int input_last;
    int run_end;
    int input_index;

    input_last = *last_index;
    if (0 < input_last) {
        previous_value = *inverse_lut;
        input_index = 1;
        output_index = -1;
        run_end = -1;
        do {
            if (previous_value != inverse_lut[input_index]) {
                run_end = input_index + -1;
                output_index = output_index + 1;
                output_lut[output_index] = *(unsigned int *)(input_lut + (long)run_end * 4);
                inverse_lut[output_index] = previous_value;
                previous_value = inverse_lut[input_index];
                input_last = *last_index;
            }
            input_index = input_index + 1;
        } while (input_index <= input_last);
        if (-1 < output_index) {
            output_index = output_index + 1;
            output_lut[output_index] = *(unsigned int *)(input_lut + (long)(run_end + 1) * 4);
            inverse_lut[output_index] = previous_value;
            *last_index = output_index;
            return;
        }
    }
    *output_lut = *(unsigned int *)(input_lut + (long)input_last * 4);
    *last_index = 0;
    return;
}
