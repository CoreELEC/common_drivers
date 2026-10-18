/* Display-management trim processing. */
#include "dm_bindings.h"
#include "dm_layout.h"
#include "dm_map_workspace.h"

void dovi_dm_FindTrimIdxUsed(int *trim_header, long metadata, long signal, long context,
                             unsigned int *indices, int *index_count)
{
    _Alignas(16) struct dm_trim_selection_workspace scratch;
    _Bool one_kept;
    _Bool two_kept;
    _Bool three_kept;
    unsigned long kept_count;
    unsigned long final_kept_count;
    unsigned int *lower_index_pointer;
    unsigned int chroma_pick;
    unsigned int chroma_peer;
    int output_count;
    unsigned int luma_pick;
    unsigned int luma_peer;

    scratch.selection.pair = 0;
    scratch.kept.indices[0] = 0;
    scratch.kept.indices[1] = 0;
    scratch.indices.pairs[2] = 0;
    scratch.indices.pairs[3] = 0;
    scratch.indices.pairs[0] = 0;
    scratch.indices.pairs[1] = 0;
    scratch.coordinates.pairs[2] = 0;
    scratch.coordinates.pairs[3] = 0;
    scratch.coordinates.pairs[0] = 0;
    scratch.coordinates.pairs[1] = 0;
    scratch.kept.pairs[1] = 0;
    if ((*trim_header != 2) && (*trim_header != 8)) {
        _printk(dovi_dm_assertion_format,
                "../dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmCfgFxp.c",
                "FindTrimIdxUsed", 0x553, "pTrimHdr->TrimLevel == 2 || pTrimHdr->TrimLevel == 8");
        dump_stack();
    }
    *index_count = 0;
    dovi_dm_BuildLumaXInfo(
        (int)(1), (unsigned long)((unsigned long)*(unsigned int *)(context + 0x43bc)),
        (long)(context + 8), (unsigned int)(0xffffffff), (int)(1), (long)(metadata), (long)(signal),
        (int *)(trim_header), (long)((long)&scratch.coordinates.pairs[0]),
        (long)((long)&scratch.indices.pairs[0]),
        (unsigned int *)((unsigned int *)((long)&scratch.selection.pair + 4)),
        (int *)((int *)&scratch.selection.pair));
    dovi_SortAscend((void *)(&scratch.coordinates.pairs[0]), (void *)(&scratch.indices.pairs[0]),
               (unsigned int)((*(unsigned int *)((unsigned char *)&scratch.selection.pair + 4))));
    luma_pick = 0;
    if ((*(unsigned int *)((unsigned char *)&scratch.selection.pair + 4)) == 0) {
        lower_index_pointer = (unsigned int *)&scratch.indices.pairs[0];
    } else {
        lower_index_pointer = (unsigned int *)&scratch.indices.pairs[0];
        if ((int)scratch.coordinates.pairs[0] < (int)scratch.selection.pair) {
            if (((*(unsigned int *)((unsigned char *)&scratch.selection.pair + 4)) == 8) ||
                (0x20 <
                 (unsigned long)((long)(int)(*(
                                     unsigned int *)((unsigned char *)&scratch.selection.pair +
                                                     4)) *
                                 4)))
                goto invalid_coordinate_count;
            luma_pick = (*(unsigned int *)((unsigned char *)&scratch.selection.pair + 4));
            luma_peer = (*(unsigned int *)((unsigned char *)&scratch.selection.pair + 4));
            if ((int)scratch.selection.pair <=
                *(int *)((long)&scratch.coordinates.pairs[0] +
                         (long)(int)(*(unsigned int *)((unsigned char *)&scratch.selection.pair +
                                                       4)) *
                             4)) {
                if ((int)(*(unsigned int *)((unsigned char *)&scratch.coordinates.pairs[0] + 4)) <
                    (int)scratch.selection.pair) {
                    if ((int)scratch.coordinates.pairs[1] < (int)scratch.selection.pair) {
                        if ((int)(*(unsigned int *)((unsigned char *)&scratch.coordinates.pairs[1] +
                                                    4)) < (int)scratch.selection.pair) {
                            if ((int)scratch.coordinates.pairs[2] < (int)scratch.selection.pair) {
                                if ((int)(*(unsigned int *)((unsigned char *)&scratch.coordinates
                                                                .pairs[2] +
                                                            4)) < (int)scratch.selection.pair) {
                                    if ((int)scratch.coordinates.pairs[3] <
                                        (int)scratch.selection.pair) {
                                        if ((int)(*(unsigned int *)((unsigned char *)&scratch
                                                                        .coordinates.pairs[3] +
                                                                    4)) <
                                            (int)scratch.selection.pair)
                                            goto invalid_coordinate_count;
                                        luma_pick = 7;
                                        luma_peer = 6;
                                    } else {
                                        luma_pick = 6;
                                        luma_peer = 5;
                                    }
                                } else {
                                    luma_pick = 5;
                                    luma_peer = 4;
                                }
                            } else {
                                luma_pick = 4;
                                luma_peer = 3;
                            }
                        } else {
                            luma_pick = 3;
                            luma_peer = 2;
                        }
                    } else {
                        luma_pick = 2;
                        luma_peer = 1;
                    }
                } else {
                    luma_peer = 0;
                    luma_pick = 1;
                }
                if ((int)(int)(*(unsigned int *)((unsigned char *)&scratch.selection.pair + 4)) <
                    (int)luma_pick) {
                    _printk(dovi_dm_assertion_format,
                            "../dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmCfgFxp.c",
                            "FindKeptIdx", 0x533, "*pIdxR <= tIdxMax");
                    dump_stack();
                }
            }
            if ((7 < luma_peer) || (7 < luma_pick))
                goto invalid_kept_index;
            lower_index_pointer = (unsigned int *)((long)&scratch.indices.pairs[0] + (unsigned long)luma_peer * 4);
        }
    }
    luma_peer = *(unsigned int *)((long)&scratch.indices.pairs[0] + (unsigned long)luma_pick * 4);
    luma_pick = *lower_index_pointer;
    if (*trim_header == 8) {
        dovi_dm_BuildChromaXInfo(
            (int)(1), (unsigned long)((unsigned long)*(unsigned int *)(context + 0x43bc)),
            (long)(context + 8), (unsigned int)(0xffffffff), (int)(1), (long)(metadata),
            (long)(signal), (int *)(trim_header), (long)((long)&scratch.coordinates.pairs[0]),
            (long)((long)&scratch.indices.pairs[0]),
            (unsigned int *)((unsigned int *)((long)&scratch.selection.pair + 4)),
            (unsigned int *)((unsigned int *)&scratch.selection.pair));
        dovi_SortAscend(
            (void *)(&scratch.coordinates.pairs[0]), (void *)(&scratch.indices.pairs[0]),
            (unsigned int)((*(unsigned int *)((unsigned char *)&scratch.selection.pair + 4))));
        chroma_pick = 0;
        if ((*(unsigned int *)((unsigned char *)&scratch.selection.pair + 4)) == 0) {
            lower_index_pointer = (unsigned int *)&scratch.indices.pairs[0];
        } else {
            lower_index_pointer = (unsigned int *)&scratch.indices.pairs[0];
            if ((int)scratch.coordinates.pairs[0] < (int)scratch.selection.pair) {
                if (((*(unsigned int *)((unsigned char *)&scratch.selection.pair + 4)) == 8) ||
                    (0x20 <
                     (unsigned long)((long)(int)(*(
                                         unsigned int *)((unsigned char *)&scratch.selection.pair +
                                                         4)) *
                                     4))) {
                invalid_coordinate_count:
                    dm_limit_trap();
                }
                chroma_pick = (*(unsigned int *)((unsigned char *)&scratch.selection.pair + 4));
                chroma_peer = (*(unsigned int *)((unsigned char *)&scratch.selection.pair + 4));
                if ((int)scratch.selection.pair <=
                    *(int *)((long)&scratch.coordinates.pairs[0] +
                             (long)(int)(*(
                                 unsigned int *)((unsigned char *)&scratch.selection.pair + 4)) *
                                 4)) {
                    if ((int)(*(unsigned int *)((unsigned char *)&scratch.coordinates.pairs[0] +
                                                4)) < (int)scratch.selection.pair) {
                        if ((int)scratch.coordinates.pairs[1] < (int)scratch.selection.pair) {
                            if ((int)(*(
                                    unsigned int *)((unsigned char *)&scratch.coordinates.pairs[1] +
                                                    4)) < (int)scratch.selection.pair) {
                                if ((int)scratch.coordinates.pairs[2] <
                                    (int)scratch.selection.pair) {
                                    if ((int)(*(unsigned int *)((unsigned char *)&scratch
                                                                    .coordinates.pairs[2] +
                                                                4)) < (int)scratch.selection.pair) {
                                        if ((int)scratch.coordinates.pairs[3] <
                                            (int)scratch.selection.pair) {
                                            if ((int)(*(unsigned int *)((unsigned char *)&scratch
                                                                            .coordinates.pairs[3] +
                                                                        4)) <
                                                (int)scratch.selection.pair)
                                                goto invalid_coordinate_count;
                                            chroma_pick = 7;
                                            chroma_peer = 6;
                                        } else {
                                            chroma_pick = 6;
                                            chroma_peer = 5;
                                        }
                                    } else {
                                        chroma_pick = 5;
                                        chroma_peer = 4;
                                    }
                                } else {
                                    chroma_pick = 4;
                                    chroma_peer = 3;
                                }
                            } else {
                                chroma_pick = 3;
                                chroma_peer = 2;
                            }
                        } else {
                            chroma_pick = 2;
                            chroma_peer = 1;
                        }
                    } else {
                        chroma_peer = 0;
                        chroma_pick = 1;
                    }
                    if ((int)(int)(*(unsigned int *)((unsigned char *)&scratch.selection.pair +
                                                     4)) < (int)chroma_pick) {
                        _printk(
                            dovi_dm_assertion_format,
                            "../dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmCfgFxp.c",
                            "FindKeptIdx", 0x533, "*pIdxR <= tIdxMax");
                        dump_stack();
                    }
                }
                if ((7 < chroma_peer) || (7 < chroma_pick))
                    goto invalid_kept_index;
                lower_index_pointer =
                    (unsigned int *)((long)&scratch.indices.pairs[0] + (unsigned long)chroma_peer * 4);
            }
        }
        chroma_peer = *(unsigned int *)((long)&scratch.indices.pairs[0] + (unsigned long)chroma_pick * 4);
        chroma_pick = *lower_index_pointer;
        if (luma_peer == luma_pick) {
            kept_count = 1;
        } else {
            kept_count = 2;
            scratch.kept.indices[1] = luma_peer;
        }
        if ((luma_pick != chroma_pick) && ((luma_peer == luma_pick || (scratch.kept.indices[1] != chroma_pick)))) {
            scratch.kept.indices[kept_count] = chroma_pick;
            kept_count = (unsigned long)((int)kept_count + 1);
        }
        if ((luma_pick == chroma_peer) ||
            ((kept_count != 1 &&
              ((scratch.kept.indices[1] == chroma_peer ||
                ((kept_count != 2 && ((unsigned int)scratch.kept.pairs[1] == chroma_peer)))))))) {
            scratch.coordinates.pairs[0] = CONCAT44(
                (*(unsigned int *)((unsigned char *)&scratch.coordinates.pairs[0] + 4)), luma_pick);
            final_kept_count = kept_count;
            if ((int)kept_count == 1)
                goto select_one_index;
        } else {
            final_kept_count = (unsigned long)((int)kept_count + 1);
            scratch.kept.indices[kept_count] = chroma_peer;
            scratch.coordinates.pairs[0] = (unsigned long)luma_pick;
        }
    order_kept_indices:
        scratch.coordinates.pairs[0] =
            CONCAT44(scratch.kept.indices[1], (int)scratch.coordinates.pairs[0]);
        if ((int)final_kept_count == 2) {
            one_kept = 0;
            three_kept = 0;
            output_count = 2;
            two_kept = 1;
            scratch.indices.pairs[0] = 0x100000000;
        } else {
            one_kept = 0;
            two_kept = 0;
            scratch.coordinates.pairs[1] =
                CONCAT44((*(unsigned int *)((unsigned char *)&scratch.coordinates.pairs[1] + 4)),
                         (unsigned int)scratch.kept.pairs[1]);
            if ((int)final_kept_count == 3) {
                output_count = 3;
                three_kept = 1;
                scratch.indices.pairs[0] = 0x100000000;
                scratch.indices.pairs[1] = CONCAT44(
                    (*(unsigned int *)((unsigned char *)&scratch.indices.pairs[1] + 4)), 2);
            } else {
                three_kept = 0;
                output_count = 4;
                scratch.coordinates.pairs[1] = scratch.kept.pairs[1];
                scratch.indices.pairs[0] = 0x100000000;
                scratch.indices.pairs[1] = 0x300000002;
            }
        }
    } else {
        if (luma_peer != luma_pick) {
            final_kept_count = 2;
            scratch.coordinates.pairs[0] = (unsigned long)luma_pick;
            scratch.kept.indices[1] = luma_peer;
            goto order_kept_indices;
        }
        scratch.coordinates.pairs[0] = CONCAT44(
            (*(unsigned int *)((unsigned char *)&scratch.coordinates.pairs[0] + 4)), luma_pick);
    select_one_index:
        two_kept = 0;
        three_kept = 0;
        one_kept = 1;
        output_count = 1;
        scratch.indices.pairs[0] = scratch.indices.pairs[0] & 0xffffffff00000000;
    }
    scratch.kept.indices[0] = luma_pick;
    dovi_SortAscend((void *)(&scratch.coordinates.pairs[0]), (void *)(&scratch.indices.pairs[0]),
               (unsigned int)(output_count + -1));
    *index_count = output_count;
    if ((unsigned int)scratch.indices.pairs[0] < 4) {
        *indices = scratch.kept.indices[scratch.indices.pairs[0] & 0xffffffff];
        if (!one_kept) {
            if (3 < (*(unsigned int *)((unsigned char *)&scratch.indices.pairs[0] + 4)))
                goto invalid_kept_index;
            indices[1] =
                scratch.kept
                    .indices[(*(unsigned int *)((unsigned char *)&scratch.indices.pairs[0] + 4))];
            if (!two_kept) {
                if (3 < (unsigned int)scratch.indices.pairs[1])
                    goto invalid_kept_index;
                indices[2] = scratch.kept.indices[scratch.indices.pairs[1] & 0xffffffff];
                if (!three_kept) {
                    if (3 < (*(unsigned int *)((unsigned char *)&scratch.indices.pairs[1] + 4)))
                        goto invalid_kept_index;
                    indices[3] = scratch.kept.indices[(
                        *(unsigned int *)((unsigned char *)&scratch.indices.pairs[1] + 4))];
                }
            }
        }

        return;
    }
invalid_kept_index:
    dm_limit_trap();
}

void dovi_dm_SetDefaultL8Trim(long tone_control, long trim)
{
    *(short *)(trim + 2) = (short)*(unsigned int *)(tone_control + 0x84) -
                           (short)*(unsigned int *)(tone_control + 0xd8);
    *(short *)(trim + 4) = (short)*(unsigned int *)(tone_control + 0x88) -
                           (short)*(unsigned int *)(tone_control + 0xdc);
    *(short *)(trim + 6) = (short)*(unsigned int *)(tone_control + 0x8c) -
                           (short)*(unsigned int *)(tone_control + 0xe0);
    *(short *)(trim + 8) = (short)*(unsigned int *)(tone_control + 0x90) -
                           (short)*(unsigned int *)(tone_control + 0xe4);
    *(short *)(trim + 10) = (short)*(unsigned int *)(tone_control + 0x94) -
                            (short)*(unsigned int *)(tone_control + 0xe8);
    *(short *)(trim + 0xc) = (short)*(unsigned int *)(tone_control + 0x98) -
                             (short)*(unsigned int *)(tone_control + 0xec);
    *(short *)(trim + 0xe) = (short)*(unsigned int *)(tone_control + 0x9c) -
                             (short)*(unsigned int *)(tone_control + 0xf0);
    *(short *)(trim + 0x10) = (short)*(unsigned int *)(tone_control + 0xa0) -
                              (short)*(unsigned int *)(tone_control + 0xf4);
    *(unsigned short *)(trim + 0xc) =
        (unsigned short)((unsigned int)(*(int *)(tone_control + 0x98) -
                                        *(int *)(tone_control + 0xec)) >>
                         1) &
        0x7fff;
    *(short *)(trim + 0x12) = (short)*(unsigned int *)(tone_control + 0xa4) -
                              (short)*(unsigned int *)(tone_control + 0xf8);
    *(short *)(trim + 0x14) = (short)*(unsigned int *)(tone_control + 0xa8) -
                              (short)*(unsigned int *)(tone_control + 0xfc);
    *(short *)(trim + 0x16) = (short)*(unsigned int *)(tone_control + 0xac) -
                              (short)*(unsigned int *)(tone_control + 0x100);
    *(short *)(trim + 0x18) = (short)*(unsigned int *)(tone_control + 0xb0) -
                              (short)*(unsigned int *)(tone_control + 0x104);
    *(short *)(trim + 0x1a) = (short)*(unsigned int *)(tone_control + 0xb4) -
                              (short)*(unsigned int *)(tone_control + 0x108);
    *(short *)(trim + 0x1c) = (short)*(unsigned int *)(tone_control + 0xb8) -
                              (short)*(unsigned int *)(tone_control + 0x10c);
    *(short *)(trim + 0x1e) = (short)*(unsigned int *)(tone_control + 0xbc) -
                              (short)*(unsigned int *)(tone_control + 0x110);
    *(short *)(trim + 0x20) = (short)*(unsigned int *)(tone_control + 0xc0) -
                              (short)*(unsigned int *)(tone_control + 0x114);
    *(short *)(trim + 0x22) = (short)*(unsigned int *)(tone_control + 0xc4) -
                              (short)*(unsigned int *)(tone_control + 0x118);
    *(short *)(trim + 0x24) = (short)*(unsigned int *)(tone_control + 200) -
                              (short)*(unsigned int *)(tone_control + 0x11c);
    *(short *)(trim + 0x26) = (short)*(unsigned int *)(tone_control + 0xcc) -
                              (short)*(unsigned int *)(tone_control + 0x120);
    *(short *)(trim + 0x28) = (short)*(unsigned int *)(tone_control + 0xd0) -
                              (short)*(unsigned int *)(tone_control + 0x124);
    return;
}

void dovi_dm_SetDefaultL2Trim(long tone_control, long trim)
{
    *(short *)(trim + 2) = ((short)*(unsigned int *)(tone_control + 0x84) -
                            (short)*(unsigned int *)(tone_control + 0xd8)) *
                               2 +
                           -0x800;
    *(short *)(trim + 4) = ((short)*(unsigned int *)(tone_control + 0x88) -
                            (short)*(unsigned int *)(tone_control + 0xdc)) *
                               2 +
                           -0x800;
    *(short *)(trim + 6) = ((short)*(unsigned int *)(tone_control + 0x8c) -
                            (short)*(unsigned int *)(tone_control + 0xe0)) *
                               2 +
                           -0x800;
    *(short *)(trim + 8) = (short)*(unsigned int *)(tone_control + 0x90) -
                           (short)*(unsigned int *)(tone_control + 0xe4);
    *(short *)(trim + 10) = (short)*(unsigned int *)(tone_control + 0x94) -
                            (short)*(unsigned int *)(tone_control + 0xe8);
    *(unsigned short *)(trim + 0xc) =
        (unsigned short)((unsigned int)(*(int *)(tone_control + 0x98) -
                                        *(int *)(tone_control + 0xec)) >>
                         1) &
        0x7fff;
    return;
}

unsigned int dovi_dm_GetTMaxFromTrim(int *trim_header, int trim_index, long metadata)
{
    int trim_count;
    unsigned char display_id;
    unsigned int maximum_pq;
    long display;
    unsigned long remaining;
    unsigned short *display_entry;
    long display_extension;

    if (metadata == 0) {
        display_extension = 0;
        trim_count = trim_header[2];
    } else {
        display_extension = 0;
        if ((*(unsigned char *)(metadata + 1) & 4) != 0) {
            display_extension = metadata + 0x218;
        }
        trim_count = trim_header[2];
    }
    if (trim_count == 0) {
        _printk(dovi_dm_assertion_format,
                "../dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmCmmtFxp.c",
                "GetTMaxFromTrim", 0x8f1, "pTrimHdr->TrimNum");
        dump_stack();
    }
    if (*trim_header == 8) {
        display_id = *(unsigned char *)(metadata + 0x110 + (long)trim_index * 0x2a + 0xc);
        display = (long)dovi_GetDisplay((unsigned int)(display_id));
        if ((int *)(metadata + 0x110) != trim_header) {
            _printk(dovi_dm_assertion_format,
                    "../dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmCmmtFxp.c",
                    "GetTMaxFromTrim", 0x8f9, "pTrimHdr == (TrimComHdr_t *)&hMdsExt->trim8");
            dump_stack();
        }
        if (display == 0) {
            if ((display_extension != 0) &&
                (remaining = (unsigned long)*(unsigned int *)(display_extension + 8),
                 0 < (int)*(unsigned int *)(display_extension + 8))) {
                display_entry = (unsigned short *)(display_extension + 0xe);
                do {
                    if (display_entry[-1] == (unsigned short)display_id) {
                        return (unsigned int)*display_entry;
                    }
                    display_entry = display_entry + 0xc;
                    remaining = remaining - 1;
                } while (remaining != 0);
            }
            maximum_pq = 0xffffffff;
        } else {
            maximum_pq = *(unsigned int *)(display + 0xc);
        }
    } else {
        if ((int *)(metadata + 0x84) != trim_header) {
            _printk(dovi_dm_assertion_format,
                    "../dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmCmmtFxp.c",
                    "GetTMaxFromTrim", 0x909, "pTrimHdr == (TrimComHdr_t *)&hMdsExt->trim2");
            dump_stack();
        }
        maximum_pq = (unsigned int)*(unsigned short *)(metadata + (long)trim_index * 0xe + 0x90);
    }
    return maximum_pq;
}
