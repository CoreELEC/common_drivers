/* Chroma coordinates and distances for trim interpolation. */
#include "dm_bindings.h"
#include "dm_layout.h"
#include "dm_map_workspace.h"

/* The distance uses the red-primary column of the Q26 RGB-to-XYZ
 * matrix, then sums saturated absolute I, Ct and Cp differences. The
 * luminance argument contributes only its low 32 bits. */
unsigned int dovi_dm_FindChromaDist(const void *primaries, unsigned int luminance, int target_pq)
{
    _Alignas(16) int rgb_to_xyz[9] = {0};
    _Alignas(16) int ictcp[3] = {0};
    _Alignas(16) long xyz[3];
    unsigned long luminance32;

    if (primaries == (void *)0)
        return 0;

    luminance32 = luminance & 0xffffffff;
    dovi_GetRgb2XyzM33ByV8Prims(primaries, rgb_to_xyz);
    xyz[0] = (long)((long)rgb_to_xyz[0] * luminance32) >> 26;
    xyz[1] = (long)((long)rgb_to_xyz[3] * luminance32) >> 26;
    xyz[2] = (long)((long)rgb_to_xyz[6] * luminance32) >> 26;
    dovi_Xyz2IctcpDm(xyz, ictcp);

    return fixed_abs32((unsigned int)target_pq - (unsigned int)ictcp[0]) +
           fixed_abs32((unsigned int)ictcp[1]) + fixed_abs32((unsigned int)ictcp[2]);
}

void dovi_dm_BuildChromaXInfo(int coordinate_scale, unsigned long unused_state, long config,
                              unsigned int flags, int exclude_default, long metadata, long signal,
                              int *trim_header, long coordinates, long indices,
                              unsigned int *last_index, unsigned int *target_coordinate)
{
    _Alignas(16) struct dm_chroma_coordinates_workspace scratch;
    int coordinate_work;
    const unsigned char *display_primaries;
    unsigned char display_id;
    unsigned int coordinate_word;
    unsigned int coordinate_auxiliary;
    long lookup_cursor;
    const unsigned long *source_primaries;
    unsigned long distance_work;
    int absolute_ct;
    int component_work;
    const unsigned char *trim_values;
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
                "BuildChromaXInfo", 0x98c, "pTrimHdr->TrimNum");
        dump_stack();
    }
    *last_index = 0;
    *target_coordinate = 0;
    if (0 < trim_header[2]) {
        trim_cursor = 0;
        do {
            if (*trim_header == 8) {
                display_id = *(unsigned char *)(metadata + trim_cursor * 0x2a + 0x11c);
                lookup_cursor = (long)dovi_GetDisplay((unsigned int)(display_id));
                if ((int *)(metadata + 0x110) != trim_header) {
                    _printk(
                        dovi_dm_assertion_format,
                        "../dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmCmmtFxp.c",
                        "BuildChromaXInfo", 0x997, "pTrimHdr == (TrimComHdr_t *)&hMdsExt->trim8");
                    dump_stack();
                }
                if (lookup_cursor == 0) {
                    if ((scratch.level10 != 0) &&
                        (distance_work =
                             (unsigned long)fixed_load_le32((const void *)(scratch.level10 + 8)),
                         trim_values = (const unsigned char *)(scratch.level10 + 0x12),
                         0 < (int)fixed_load_le32((const void *)(scratch.level10 + 8)))) {
                        do {
                            if (((unsigned short)fixed_load_le16(trim_values - 6)) ==
                                (unsigned short)display_id) {
                                if (0x3f6 < ((unsigned short)fixed_load_le16(trim_values - 4))) {
                                    coordinate_word = (unsigned int)((unsigned short)fixed_load_le16(
                                                trim_values - 4))
                                            << 6;
                                    scratch.color.packed.pairs[2] = 0;
                                    scratch.color.packed.pairs[3] = 0;
                                    scratch.color.packed.pairs[0] = 0;
                                    scratch.color.packed.pairs[1] = 0;
                                    distance_work = dovi_Pq2L((unsigned int)(coordinate_word));
                                    if ((unsigned long)((unsigned short)fixed_load_le16(
                                            trim_values)) < 0x13) {
                                        source_primaries = (const unsigned long *)dovi_v8_primaries[(
                                            (unsigned short)fixed_load_le16(trim_values))];
                                    } else {
                                        scratch.color.packed.pairs[0] =
                                            CONCAT44((int)(short)((unsigned short)fixed_load_le16(
                                                         trim_values + 4))
                                                         << 0xb,
                                                     (int)(short)((unsigned short)fixed_load_le16(
                                                         trim_values + 2))
                                                         << 0xb);
                                        scratch.color.packed.pairs[1] =
                                            CONCAT44((int)(short)((unsigned short)fixed_load_le16(
                                                         trim_values + 8))
                                                         << 0xb,
                                                     (int)(short)((unsigned short)fixed_load_le16(
                                                         trim_values + 6))
                                                         << 0xb);
                                        scratch.color.packed.pairs[2] =
                                            CONCAT44((int)(short)((unsigned short)fixed_load_le16(
                                                         trim_values + 12))
                                                         << 0xb,
                                                     (int)(short)((unsigned short)fixed_load_le16(
                                                         trim_values + 10))
                                                         << 0xb);
                                        source_primaries = &scratch.color.packed.pairs[0];
                                        scratch.color.packed.pairs[3] =
                                            CONCAT44((int)(short)((unsigned short)fixed_load_le16(
                                                         trim_values + 16))
                                                         << 0xb,
                                                     (int)(short)((unsigned short)fixed_load_le16(
                                                         trim_values + 14))
                                                         << 0xb);
                                    }
                                    coordinate_word = dovi_dm_FindChromaDist(
                                        source_primaries, (unsigned long)(distance_work >> 6), (int)(coordinate_word));
                                    goto append_coordinate;
                                }
                                break;
                            }
                            distance_work = distance_work - 1;
                            trim_values = trim_values + 0x18;
                        } while (distance_work != 0);
                    }
                } else if (0x3f6 < *(int *)(lookup_cursor + 0xc)) {
                    display_primaries = (unsigned char *)0x0;
                    if (*(unsigned int *)(lookup_cursor + 0x10) < 0x13) {
                        display_primaries = (const unsigned char *)
                            dovi_v8_primaries[*(unsigned int *)(lookup_cursor + 0x10)];
                    }
                    coordinate_word = dovi_dm_FindChromaDist(
                        display_primaries, (unsigned long)((unsigned long)*(unsigned int *)(lookup_cursor + 8)),
                        (int)(*(int *)(lookup_cursor + 0xc)));
                    goto append_coordinate;
                }
            } else {
                trim_values = (const unsigned char *)(metadata + trim_cursor * 0xe + 0x90);
                coordinate_word = (unsigned int)((unsigned short)fixed_load_le16(trim_values)) * 0x40;
                distance_work = dovi_Pq2L((unsigned int)(coordinate_word));
                if ((int *)(metadata + 0x84) != trim_header) {
                    _printk(
                        dovi_dm_assertion_format,
                        "../dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmCmmtFxp.c",
                        "BuildChromaXInfo", 0x9b2, "pTrimHdr == (TrimComHdr_t *)&hMdsExt->trim2");
                    dump_stack();
                }
                if (0x3f6 < ((unsigned short)fixed_load_le16(trim_values))) {
                    scratch.ictcp.components[2] = 0;
                    scratch.ictcp.pair = 0;
                    scratch.color.packed.last = 0;
                    scratch.color.packed.pairs[2] = 0;
                    scratch.color.packed.pairs[3] = 0;
                    scratch.color.packed.pairs[0] = 0;
                    scratch.color.packed.pairs[1] = 0;
                    dovi_GetRgb2XyzM33ByV8Prims(dovi_v8_primaries[1],
                                           (void *)(&scratch.color.packed.pairs[0]));
                    distance_work = distance_work >> 6 & 0xffffffff;
                    scratch.xyz[0] =
                        (long)(distance_work * (long)(int)scratch.color.packed.pairs[0]) >> 0x1a;
                    scratch.xyz[1] =
                        (long)(distance_work *
                               (long)(int)(*(
                                   unsigned int *)((unsigned char *)&scratch.color.packed.pairs[1] +
                                                   4))) >>
                        0x1a;
                    scratch.xyz[2] =
                        (long)(distance_work * (long)(int)scratch.color.packed.pairs[3]) >> 0x1a;
                    dovi_Xyz2IctcpDm((const void *)(&scratch.xyz[0]), (void *)(&scratch.ictcp.pair));
                    component_work = coordinate_word - (int)scratch.ictcp.pair;
                    absolute_ct = 0x7fffffff;
                    coordinate_work = absolute_ct;
                    if (component_work != -0x80000000) {
                        coordinate_work = -component_work;
                    }
                    if (-1 < component_work) {
                        coordinate_work = component_work;
                    }
                    if ((*(unsigned int *)((unsigned char *)&scratch.ictcp.pair + 4)) !=
                        -0x80000000) {
                        absolute_ct = -(*(unsigned int *)((unsigned char *)&scratch.ictcp.pair + 4));
                    }
                    if (-1 < (int)(*(unsigned int *)((unsigned char *)&scratch.ictcp.pair + 4))) {
                        absolute_ct = (*(unsigned int *)((unsigned char *)&scratch.ictcp.pair + 4));
                    }
                    component_work = 0x7fffffff;
                    if (scratch.ictcp.components[2] != -0x80000000) {
                        component_work = -scratch.ictcp.components[2];
                    }
                    if (-1 < scratch.ictcp.components[2]) {
                        component_work = scratch.ictcp.components[2];
                    }
                    coordinate_word = coordinate_work + absolute_ct + component_work;
                append_coordinate:
                    *(unsigned int *)(coordinates + (long)(int)*last_index * 4) = coordinate_word;
                    coordinate_auxiliary = *last_index;
                    coordinate_word = *(int *)(coordinates + (long)(int)coordinate_auxiliary * 4) * coordinate_scale;
                    coordinate_work = (coordinate_word & 0xffffff00) + 0x100;
                    coordinate_word = coordinate_word + 0x80 & 0xffffff00;
                    if (0 < (int)coordinate_auxiliary) {
                        lookup_cursor = 0;
                        do {
                            if (lookup_cursor == 0x44)
                                goto coordinate_limit;
                            if ((coordinate_word ==
                                 *(unsigned int *)((long)&scratch.rounded.pairs[0] + lookup_cursor)) ||
                                (coordinate_work == *(int *)((long)&scratch.upper.pairs[0] + lookup_cursor)))
                                goto next_trim;
                            lookup_cursor = lookup_cursor + 4;
                        } while ((unsigned long)coordinate_auxiliary * 4 - lookup_cursor != 0);
                    }
                    if ((coordinate_auxiliary == 0x11) || (distance_work = (long)(int)coordinate_auxiliary * 4, 0x44 < distance_work))
                        goto coordinate_limit;
                    *(int *)((long)&scratch.upper.pairs[0] + distance_work) = coordinate_work;
                    *(unsigned int *)((long)&scratch.rounded.pairs[0] + distance_work) = coordinate_word;
                    *(int *)(indices + distance_work) = (int)trim_cursor;
                    *last_index = *last_index + 1;
                }
            }
        next_trim:
            trim_cursor = trim_cursor + 1;
        } while (trim_cursor < trim_header[2]);
    }
    if (exclude_default == 0) {
        coordinate_word = *(unsigned int *)(signal + 0x1c);
        if (0x9c3fffff < coordinate_word) {
            coordinate_word = 0x9c400000;
        }
        coordinate_word = dovi_dm_FindChromaDist((void *)((void *)(signal + 0x44)),
                                       (unsigned long)((unsigned long)coordinate_word),
                                       (int)(*(int *)(signal + 0x14) << 6));
        *(unsigned int *)(coordinates + (long)(int)*last_index * 4) = coordinate_word;
        coordinate_word = *last_index;
        if (0 < (int)coordinate_word) {
            trim_cursor = 0;
            coordinate_auxiliary = *(int *)(coordinates + (long)(int)coordinate_word * 4) * coordinate_scale;
            do {
                if (trim_cursor == 0x44) {
                coordinate_limit:
                    dm_limit_trap();
                }
                if (((coordinate_auxiliary + 0x80 & 0xffffff00) ==
                     *(unsigned int *)((long)&scratch.rounded.pairs[0] + trim_cursor)) ||
                    ((coordinate_auxiliary & 0xffffff00) + 0x100 ==
                     *(int *)((long)&scratch.upper.pairs[0] + trim_cursor))) {
                    *last_index = coordinate_word - 1;
                    goto select_target_distance;
                }
                trim_cursor = trim_cursor + 4;
            } while ((unsigned long)coordinate_word * 4 - trim_cursor != 0);
        }
        *(int *)(indices + (long)(int)coordinate_word * 4) = trim_header[2];
    select_target_distance:
        coordinate_word = *(unsigned int *)(signal + 0x88);
        if (0x9c3fffff < coordinate_word) {
            coordinate_word = 0x9c400000;
        }
        coordinate_word = dovi_dm_FindChromaDist((void *)((void *)(signal + 0xb0)),
                                       (unsigned long)((unsigned long)coordinate_word),
                                       (int)(*(int *)(signal + 0x80) << 6));
    } else {
        *last_index = *last_index - 1;
        coordinate_word = *(unsigned int *)(config + 0x164);
        coordinate_auxiliary = dovi_LToPQ12((unsigned int)(coordinate_word));
        if (0x9c3fffff < coordinate_word) {
            coordinate_word = 0x9c400000;
        }
        coordinate_word = dovi_dm_FindChromaDist((void *)((void *)(config + 0xa0)),
                                       (unsigned long)((unsigned long)coordinate_word),
                                       (int)((coordinate_auxiliary & 0xffff) << 6));
    }
    *target_coordinate = coordinate_word;

    return;
}
