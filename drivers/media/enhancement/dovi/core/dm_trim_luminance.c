/* Luminance coordinates for trim interpolation. */
#include "dm_bindings.h"
#include "dm_layout.h"
#include "dm_map_workspace.h"

void dovi_dm_BuildLumaXInfo(int coordinate_scale, unsigned long unused_state, long config,
                            unsigned int flags, int exclude_default, long metadata, long signal,
                            int *trim_header, long coordinates, long indices,
                            unsigned int *last_index, int *target_coordinate)
{
    _Alignas(16) struct dm_luma_coordinates_workspace scratch;
    unsigned char display_id;
    unsigned short maximum_pq;
    unsigned int rounded_coordinate;
    unsigned int coordinate_word;
    long coordinate_lookup;
    int coordinate_value;
    unsigned long remaining_displays;
    unsigned short *display_maximum;
    long duplicate_offset;
    long trim_cursor;

    scratch.level10 = 0;

    scratch.rounded.values[16] = 0;
    scratch.rounded.pairs[6] = 0;
    scratch.rounded.pairs[7] = 0;
    scratch.rounded.pairs[4] = 0;
    scratch.rounded.pairs[5] = 0;
    scratch.rounded.pairs[2] = 0;
    scratch.rounded.pairs[3] = 0;
    scratch.rounded.pairs[0] = 0;
    scratch.rounded.pairs[1] = 0;
    scratch.upper.values[16] = 0;
    scratch.upper.pairs[6] = 0;
    scratch.upper.pairs[7] = 0;
    scratch.upper.pairs[4] = 0;
    scratch.upper.pairs[5] = 0;
    scratch.upper.pairs[3] = 0;
    scratch.upper.pairs[1] = 0;
    scratch.upper.pairs[2] = 0;
    scratch.upper.pairs[0] = 0;
    if (((flags >> 10 & 1) != 0) && (metadata != 0)) {
        scratch.level10 = 0;
        if ((*(unsigned char *)(metadata + 1) & 4) != 0) {
            scratch.level10 = metadata + 0x218;
        }
    }
    if (trim_header[2] == 0) {
        _printk(dovi_dm_assertion_format,
                "../dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmCmmtFxp.c",
                "BuildLumaXInfo", 0x923, "pTrimHdr->TrimNum");
        dump_stack();
    }
    *last_index = 0;
    *target_coordinate = 0;
    if (0 < trim_header[2]) {
        trim_cursor = 0;
        do {
            if (*trim_header == 8) {
                display_id = *(unsigned char *)(metadata + trim_cursor * 0x2a + 0x11c);
                coordinate_lookup = (long)dovi_GetDisplay((unsigned int)(display_id));
                if ((int *)(metadata + 0x110) != trim_header) {
                    _printk(
                        dovi_dm_assertion_format,
                        "../dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmCmmtFxp.c",
                        "BuildLumaXInfo", 0x92f, "pTrimHdr == (TrimComHdr_t *)&hMdsExt->trim8");
                    dump_stack();
                }
                if (coordinate_lookup != 0) {
                    coordinate_value = *(int *)(coordinate_lookup + 0xc);
                    goto append_coordinate;
                }
                if ((scratch.level10 != 0) &&
                    (remaining_displays = (unsigned long)*(unsigned int *)(scratch.level10 + 8),
                     display_maximum = (unsigned short *)(scratch.level10 + 0xe),
                     0 < (int)*(unsigned int *)(scratch.level10 + 8))) {
                    do {
                        if (display_maximum[-1] == (unsigned short)display_id) {
                            maximum_pq = *display_maximum;
                            goto scale_maximum_pq;
                        }
                        remaining_displays = remaining_displays - 1;
                        display_maximum = display_maximum + 0xc;
                    } while (remaining_displays != 0);
                }
            } else {
                if ((int *)(metadata + 0x84) != trim_header) {
                    _printk(
                        dovi_dm_assertion_format,
                        "../dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmCmmtFxp.c",
                        "BuildLumaXInfo", 0x940, "pTrimHdr == (TrimComHdr_t *)&hMdsExt->trim2");
                    dump_stack();
                }
                maximum_pq = *(unsigned short *)(metadata + trim_cursor * 0xe + 0x90);
            scale_maximum_pq:
                coordinate_value = (unsigned int)maximum_pq << 6;
            append_coordinate:
                *(int *)(coordinates + (long)(int)*last_index * 4) = coordinate_value;
                coordinate_word = *last_index;
                coordinate_lookup = (long)(int)coordinate_word;
                coordinate_value = *(int *)(coordinates + coordinate_lookup * 4);
                if (0xfdbf < coordinate_value) {
                    rounded_coordinate = coordinate_value * coordinate_scale;
                    coordinate_value = (rounded_coordinate & 0xffffff00) + 0x100;
                    rounded_coordinate = (rounded_coordinate + 0x80) & 0xffffff00;
                    if (0 < (int)coordinate_word) {
                        duplicate_offset = 0;
                        do {
                            if (duplicate_offset == 0x44)
                                goto coordinate_limit;
                            if ((rounded_coordinate ==
                                 *(unsigned int *)((long)&scratch.rounded.pairs[0] + duplicate_offset)) ||
                                (coordinate_value == *(int *)((long)&scratch.upper.pairs[0] + duplicate_offset)))
                                goto next_trim;
                            duplicate_offset = duplicate_offset + 4;
                        } while ((unsigned long)coordinate_word * 4 - duplicate_offset != 0);
                    }
                    if ((coordinate_word == 0x11) || (0x44 < (unsigned long)(coordinate_lookup * 4)))
                        goto coordinate_limit;
                    *(int *)((long)&scratch.upper.pairs[0] + coordinate_lookup * 4) = coordinate_value;
                    *(unsigned int *)((long)&scratch.rounded.pairs[0] + coordinate_lookup * 4) = rounded_coordinate;
                    *(int *)(indices + coordinate_lookup * 4) = (int)trim_cursor;
                    *last_index = *last_index + 1;
                }
            }
        next_trim:
            trim_cursor = trim_cursor + 1;
        } while (trim_cursor < trim_header[2]);
    }
    if (exclude_default == 0) {
        *(unsigned int *)(coordinates + (long)(int)*last_index * 4) =
            (unsigned int)*(unsigned short *)(metadata + 0x4e) << 6;
        coordinate_word = *last_index;
        if (0 < (int)coordinate_word) {
            trim_cursor = 0;
            rounded_coordinate = *(int *)(coordinates + (long)(int)coordinate_word * 4) * coordinate_scale;
            do {
                if (trim_cursor == 0x44) {
                coordinate_limit:
                    dm_limit_trap();
                }
                if ((((rounded_coordinate + 0x80) & 0xffffff00) ==
                     *(unsigned int *)((long)&scratch.rounded.pairs[0] + trim_cursor)) ||
                    ((rounded_coordinate & 0xffffff00) + 0x100 ==
                     *(int *)((long)&scratch.upper.pairs[0] + trim_cursor))) {
                    *last_index = coordinate_word - 1;
                    goto select_target_pq;
                }
                trim_cursor = trim_cursor + 4;
            } while ((unsigned long)coordinate_word * 4 - trim_cursor != 0);
        }
        *(int *)(indices + (long)(int)coordinate_word * 4) = trim_header[2];
    select_target_pq:
        coordinate_word = *(unsigned int *)(signal + 0x80);
    } else {
        *last_index = *last_index - 1;
        coordinate_word = dovi_LToPQ12((unsigned int)(*(unsigned int *)(config + 0x164)));
        coordinate_word = coordinate_word & 0xffff;
    }
    *target_coordinate = coordinate_word << 6;

    return;
}
