/* Output display metadata selection and serialization. */
#include "dm_bindings.h"
#include "dm_layout.h"
#include "dm_workspace.h"
#include "fixed_access.h"

/* Identity selection preserves the native signed count and partial writes
 * before the fatal16-entry bound. Unselected workspace entries remain intact. */
static void initialize_trim_indices(struct dm_output_workspace *work, unsigned int count)
{
    unsigned int index;
    for (index = 0; (int)index < (int)count; ++index) {
        if (index == 16)
            dm_limit_trap();
        work->trims.indices[index] = index;
    }
}

/* Select extension blocks before updating the output metadata buffer. */
static unsigned int output_metadata_exclusions(const unsigned int *source, unsigned int flags,
                                               int mode, int target_version,
                                               unsigned int output_mode, const void *signal_context,
                                               const void *context)
{
    const struct dm_control *control = &((const struct dm_context_header *)context)->control;
    unsigned int excluded;
    int level11_mode;

    if (control->disable_output_metadata == 0) {
        if (output_mode == 1) {
            if ((((3 < target_version && 3 < *(int *)((long)signal_context + 0xe2a8)) &&
                  mode != 0) ||
                 (source == (unsigned int *)0x0)) ||
                (((unsigned char)*source >> 2 & 1) == 0)) {
                flags = flags | 4;
            }
            if (target_version < 4) {
                flags = flags | 8;
            }
            if (((mode != 0) && (control->preserve_level4 == 0)) ||
                ((source == (unsigned int *)0x0 || (((unsigned char)*source >> 4 & 1) == 0)))) {
                flags = flags | 0x10;
            }
            if (((target_version < 4) || (*(int *)((long)signal_context + 0xe2a8) < 4)) ||
                (source == (unsigned int *)0x0)) {
                if (target_version < 4) {
                    excluded = flags | 0x700;
                } else {
                    excluded = flags | 0x500;
                    if ((source != (unsigned int *)0x0) && ((*source >> 0x1e & 1) != 0))
                        goto select_level11;
                }
                goto exclude_level254;
            }
            excluded = ((*source ^ 0xffffffff) & 0x500) | flags;
            if ((*source >> 0x1e & 1) == 0)
                goto exclude_level254;
        } else {
            if (target_version < 4) {
                excluded = flags | 0x71c;
            } else {
                excluded = flags | 0x51c;
                if (output_mode < 2)
                    goto select_level11;
            }
        exclude_level254:
            excluded = excluded | 0x40000000;
            if (target_version < 4)
                goto exclude_level11;
        }
    select_level11:
        level11_mode = control->level11.mode;
        if (level11_mode == 0)
            goto exclude_level11;
        if (level11_mode == 2) {
            if (control->level11.override == 0)
                goto require_source_level11;
            if ((((control->level11.content_type == 0) && (control->level11.white_point == 0)) &&
                 (control->level11.reference_mode == 0)) &&
                ((control->level11.byte2 == 0 && (control->level11.byte3 == 0))))
                goto exclude_level11;
        } else if (level11_mode == 1) {
        require_source_level11:
            if ((source == (unsigned int *)0x0) ||
                ((*(unsigned char *)((long)source + 1) >> 3 & 1) == 0))
                goto exclude_level11;
        }
    } else {
        excluded = flags | 0x4000073e;
        if (3 < target_version)
            goto select_level11;
    exclude_level11:
        excluded = excluded | 0x800;
    }
    return excluded;
}

/* Prepare the L11 payload; serialization and capacity accounting stay in
 * GenerateOutputMd so partial output retains the same write boundaries. */
static void prepare_level11_metadata(const unsigned int *source, void *context)
{
    const struct dm_control *control = &((const struct dm_context_header *)context)->control;
    struct dm_metadata_format *generated = (void *)((long)context + 0x90c);
    unsigned char reference_mode;

    if (control->level11.mode == 1) {
    copy_source_level11:
        if (*(int *)((long)context + 0x4398) == 0) {
            _printk(dovi_dm_assertion_format,
                    "../dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmCfgFxp.c",
                    "GenerateOutputMd", 0x862, "hDm->l11Len");
            dump_stack();
        }
        generated->level11_white_point = (char)source[0x1b];
        generated->level11_content_type = *(unsigned char *)((long)source + 0x6d);
        reference_mode = *(unsigned char *)((long)source + 0x6e);
    store_reference_mode:
        generated->level11_reference_mode = reference_mode;
    } else if (control->level11.mode == 2) {
        if (control->level11.override == 0)
            goto copy_source_level11;
        reference_mode = (unsigned char)(unsigned int)control->level11.reference_mode;
        generated->level11_white_point = 0;
        *(unsigned int *)((long)context + 0x4398) = 4;
        *(unsigned char *)((long)context + 0x439c) =
            *(unsigned char *)&control->level11.content_type;
        generated->level11_content_type = *(unsigned char *)&control->level11.content_type;
        *(char *)((long)context + 0x439e) = (char)(unsigned int)control->level11.byte2;
        *(char *)((long)context + 0x439f) = (char)(unsigned int)control->level11.byte3;
        *(unsigned char *)((long)context + 0x439d) =
            reference_mode | (unsigned char)(control->level11.white_point << 4);
        goto store_reference_mode;
    }
}

int dovi_dm_GetMdsOutput(unsigned char *output, unsigned int capacity, unsigned int format,
                         unsigned int flags, unsigned int mode, void *context)
{
    const struct dm_context_header *header = context;
    const struct dm_control *control;
    _Bool measure_output;
    int output_size;

    if ((context != (void *)0x0) && (header->initialized != 0)) {
        control = &header->control;
        if ((int)mode < 0) {
            mode = control->output_metadata_mode;
        }
        if (output == (unsigned char *)0x0) {
            capacity = 0;
        } else {
            if ((int)capacity < 1) {
                capacity = control->output_metadata_capacity;
            }
            measure_output = mode != 0;
            mode = 0;
            if (measure_output) {
                output_size = dovi_dm_GenerateOutputMd((unsigned char *)0x0, (unsigned int)0,
                                                       (unsigned int)format, (unsigned int)flags,
                                                       (int)0, (void *)context);
                mode = (unsigned int)((int)control->output_metadata_capacity < output_size);
            }
        }
        output_size = dovi_dm_GenerateOutputMd((unsigned char *)output, (unsigned int)capacity,
                                               (unsigned int)format, (unsigned int)flags, (int)mode,
                                               (void *)context);
        return output_size;
    }
    return -1;
}

struct metadata_output_cursor {
    unsigned char *count;
    unsigned char *records;
    unsigned long remaining;
};

/* Prepare and serialize the fixed header before extension selection. Capacity
 * checks remain interleaved with context updates to preserve partial output. */
static int write_output_metadata_header(unsigned char *output, unsigned int capacity,
                                        unsigned int *source, unsigned int output_mode, void *signal_context,
                                        long kernel_map, int header_control_word, void *context,
                                        struct metadata_output_cursor *cursor)
{
    struct dm_metadata_format *generated = (void *)((long)context + 0x90c);
    unsigned int *destination = (void *)generated;
    unsigned int *signal_words = (void *)((long)signal_context + 0xe240);
    _Alignas(8) unsigned short matrix[9] = {0};
    _Bool synthetic_header = header_control_word != 2;
    _Bool missing_source = source == (void *)0;
    unsigned char chroma_code;
    unsigned char major_version;
    unsigned int swapped_eotf_range;
    unsigned int capacity_or_endian_word;
    unsigned char chroma_work;
    unsigned char range_or_chroma_code;
    unsigned short selected_eotf_offset;
    unsigned short signal_halfword;
    unsigned int remaining_after_matrices;
    unsigned int selected_eotf_range;

    if (synthetic_header || missing_source) {
        if ((source == (unsigned int *)0x0) || (header_control_word == 2))
            goto initialize_header_version;
        major_version = (unsigned char)source[1];
        generated->version_major = major_version;
        generated->version_minor = *(unsigned char *)((long)source + 5);
    } else {
        dovi_MemCpyByte((void *)destination, (const void *)source, (int)0x52);
    initialize_header_version:
        major_version = '\0';
        generated->version_major = 0;
        generated->version_minor = 0;
    }
    if ((capacity != 0) && (*output = major_version, capacity != 1)) {
        output[1] = generated->version_minor;
        generated->ycc_to_rgb_scale = 0xd;
        generated->rgb_to_lms_scale = 0xe;
        if (synthetic_header || missing_source) {
            dovi_InvM33((const void *)((void *)(kernel_map + 0x40)), (void *)matrix,
                   (unsigned int)((unsigned int)*(unsigned short *)(kernel_map + 0x3e)),
                   (unsigned int)0xd);
            *(int *)((long)context + 0x928) = *(int *)(kernel_map + 0x54) << 0x10;
            *(int *)((long)context + 0x92c) = *(int *)(kernel_map + 0x58) << 0x10;
            header_control_word = *(int *)(kernel_map + 0x5c);
            fixed_store_le32((unsigned char *)context + 0x936,
                             *(unsigned int *)((long)context + 0x440c));
            *(unsigned int *)((long)context + 0x914) = *(unsigned int *)&matrix[0];
            *(unsigned long *)((long)context + 0x918) =
                CONCAT44(*(unsigned int *)&matrix[4], *(unsigned int *)&matrix[2]);
            *(unsigned int *)((long)context + 0x920) = *(unsigned int *)&matrix[6];
            generated->ycc_to_rgb[8] = matrix[8];
            generated->rgb_to_lms[2] = *(unsigned short *)((long)context + 0x4410);
            generated->rgb_to_lms[3] = *(unsigned short *)((long)context + 0x4412);
            generated->rgb_to_lms[4] = *(unsigned short *)((long)context + 0x4414);
            generated->rgb_to_lms[5] = *(unsigned short *)((long)context + 0x4416);
            generated->rgb_to_lms[6] = *(unsigned short *)((long)context + 0x4418);
            generated->rgb_to_lms[7] = *(unsigned short *)((long)context + 0x441a);
            *(int *)((long)context + 0x930) = header_control_word << 0x10;
            generated->rgb_to_lms[8] = *(unsigned short *)((long)context + 0x441c);
        }
        if (0x11 < (int)(capacity - 2)) {
            fixed_store_le16(output + 2,
                             generated->ycc_to_rgb[0] >> 8 | generated->ycc_to_rgb[0] << 8);
            fixed_store_le16(output + 4,
                             generated->ycc_to_rgb[1] >> 8 | generated->ycc_to_rgb[1] << 8);
            fixed_store_le16(output + 6,
                             generated->ycc_to_rgb[2] >> 8 | generated->ycc_to_rgb[2] << 8);
            fixed_store_le16(output + 8,
                             generated->ycc_to_rgb[3] >> 8 | generated->ycc_to_rgb[3] << 8);
            fixed_store_le16(output + 10,
                             generated->ycc_to_rgb[4] >> 8 | generated->ycc_to_rgb[4] << 8);
            fixed_store_le16(output + 0xc,
                             generated->ycc_to_rgb[5] >> 8 | generated->ycc_to_rgb[5] << 8);
            fixed_store_le16(output + 0xe,
                             generated->ycc_to_rgb[6] >> 8 | generated->ycc_to_rgb[6] << 8);
            fixed_store_le16(output + 0x10,
                             generated->ycc_to_rgb[7] >> 8 | generated->ycc_to_rgb[7] << 8);
            fixed_store_le16(output + 0x12,
                             generated->ycc_to_rgb[8] >> 8 | generated->ycc_to_rgb[8] << 8);
            if (capacity - 0x20 < 0xfffffff2) {
                capacity_or_endian_word = (generated->ycc_offsets[0] & 0xff00ff00) >> 8 |
                         (generated->ycc_offsets[0] & 0xff00ff) << 8;
                fixed_store_le32(output + 0x14, capacity_or_endian_word >> 0x10 | capacity_or_endian_word << 0x10);
                capacity_or_endian_word = (generated->ycc_offsets[1] & 0xff00ff00) >> 8 |
                         (generated->ycc_offsets[1] & 0xff00ff) << 8;
                fixed_store_le32(output + 0x18, capacity_or_endian_word >> 0x10 | capacity_or_endian_word << 0x10);
                capacity_or_endian_word = (generated->ycc_offsets[2] & 0xff00ff00) >> 8 |
                         (generated->ycc_offsets[2] & 0xff00ff) << 8;
                remaining_after_matrices = capacity - 0x32;
                fixed_store_le32(output + 0x1c, capacity_or_endian_word >> 0x10 | capacity_or_endian_word << 0x10);
                if (remaining_after_matrices < 0xffffffea) {
                    fixed_store_le16(output + 0x20,
                                     generated->rgb_to_lms[0] >> 8 | generated->rgb_to_lms[0] << 8);
                    fixed_store_le16(output + 0x22,
                                     generated->rgb_to_lms[1] >> 8 | generated->rgb_to_lms[1] << 8);
                    fixed_store_le16(output + 0x24,
                                     generated->rgb_to_lms[2] >> 8 | generated->rgb_to_lms[2] << 8);
                    fixed_store_le16(output + 0x26,
                                     generated->rgb_to_lms[3] >> 8 | generated->rgb_to_lms[3] << 8);
                    fixed_store_le16(output + 0x28,
                                     generated->rgb_to_lms[4] >> 8 | generated->rgb_to_lms[4] << 8);
                    fixed_store_le16(output + 0x2a,
                                     generated->rgb_to_lms[5] >> 8 | generated->rgb_to_lms[5] << 8);
                    fixed_store_le16(output + 0x2c,
                                     generated->rgb_to_lms[6] >> 8 | generated->rgb_to_lms[6] << 8);
                    fixed_store_le16(output + 0x2e,
                                     generated->rgb_to_lms[7] >> 8 | generated->rgb_to_lms[7] << 8);
                    fixed_store_le16(output + 0x30,
                                     generated->rgb_to_lms[8] >> 8 | generated->rgb_to_lms[8] << 8);
                    if (synthetic_header || missing_source) {
                        if (*(char *)(kernel_map + 0x3a) == '\x01') {
                            signal_halfword = 0xffff;
                        } else {
                            signal_halfword = (unsigned short)*(unsigned int *)((long)context + 0x164);
                        }
                        generated->eotf = signal_halfword;
                        if (*(char *)(kernel_map + 0x3a) == '\x01') {
                            signal_halfword = 0;
                            selected_eotf_offset = 0;
                            selected_eotf_range = 0;
                        } else {
                            signal_halfword = *(unsigned short *)((long)context + 0x174);
                            selected_eotf_offset = *(unsigned short *)((long)context + 0x176);
                            selected_eotf_range = *(unsigned int *)((long)context + 0x178);
                        }
                        generated->eotf_scale = signal_halfword;
                        generated->eotf_offset = selected_eotf_offset;
                        generated->eotf_range = selected_eotf_range;
                    }
                    if ((capacity & 0xfffffffc) != 0x30) {
                        capacity_or_endian_word = remaining_after_matrices & 0xfffffffe;
                        fixed_store_le16(output + 0x32,
                                         generated->eotf >> 8 | generated->eotf << 8);
                        if (((capacity_or_endian_word != 2) &&
                             (fixed_store_le16(output + 0x34, generated->eotf_scale >> 8 |
                                                                  generated->eotf_scale << 8),
                              capacity_or_endian_word != 4)) &&
                            (fixed_store_le16(output + 0x36, generated->eotf_offset >> 8 |
                                                                 generated->eotf_offset << 8),
                             capacity - 0x3c < 0xfffffffc)) {
                            swapped_eotf_range = (generated->eotf_range & 0xff00ff00) >> 8 |
                                     (generated->eotf_range & 0xff00ff) << 8;
                            fixed_store_le32(output + 0x38, swapped_eotf_range >> 0x10 | swapped_eotf_range << 0x10);
                            if (synthetic_header || missing_source) {
                                header_control_word = *(int *)((long)context + 0x84);
                                *(char *)((long)context + 0x954) =
                                    (char)*(unsigned int *)((long)context + 0x94);
                                if (header_control_word - 3U < 2) {
                                    generated->color_space = 2;
                                    if ((header_control_word == 4) && (*(int *)((long)context + 0x98) != 0)) {
                                    set_yuv_color:
                                        generated->color_space = 0;
                                    }
                                    range_or_chroma_code = 1;
                                    chroma_work = 1;
                                    if (*(int *)((long)context + 0x88) != 1) {
                                        chroma_work = 2;
                                    }
                                    chroma_code = 0;
                                    if (*(int *)((long)context + 0x88) != 0) {
                                        chroma_code = chroma_work;
                                    }
                                    generated->chroma_format = chroma_code;
                                } else {
                                    if (header_control_word == 0)
                                        goto set_yuv_color;
                                    generated->color_space = 1;
                                    range_or_chroma_code = 1;
                                    if (*(int *)((long)context + 0x88) != 1) {
                                        range_or_chroma_code = 2;
                                    }
                                    chroma_work = 0;
                                    if (*(int *)((long)context + 0x88) != 0) {
                                        chroma_work = range_or_chroma_code;
                                    }
                                    generated->chroma_format = chroma_work;
                                    if (*(int *)((long)context + 0x110) == 0) {
                                        range_or_chroma_code = 0;
                                    } else {
                                        range_or_chroma_code = 1;
                                        if (*(int *)((long)context + 0x110) != 1) {
                                            range_or_chroma_code = 2;
                                        }
                                    }
                                }
                                generated->full_range = range_or_chroma_code;
                            }
                            if (((remaining_after_matrices != 10) &&
                                 (output[0x3c] = generated->bit_depth, remaining_after_matrices != 0xb)) &&
                                ((output[0x3d] = generated->color_space,
                                  remaining_after_matrices != 0xc &&
                                      (output[0x3e] = generated->chroma_format, remaining_after_matrices != 0xd)))) {
                                output[0x3f] = generated->full_range;
                                if (synthetic_header || missing_source) {
                                    if (output_mode == 1) {
                                        if (source == (unsigned int *)0x0) {
                                            *(short *)((long)context + 0x958) = (short)*signal_words;
                                            signal_halfword = 0x2a;
                                            *(short *)((long)context + 0x95a) =
                                                (short)*(unsigned int *)((long)signal_context + 0xe244);
                                        } else {
                                            *(short *)((long)context + 0x958) = (short)source[0x13];
                                            generated->source_max_pq =
                                                *(unsigned short *)((long)source + 0x4e);
                                        copy_source_diagonal:
                                            signal_halfword = (unsigned short)source[0x14];
                                        }
                                    } else {
                                        *(short *)((long)context + 0x958) =
                                            (short)*(unsigned int *)((long)signal_context + 0xe2ac);
                                        *(short *)((long)context + 0x95a) =
                                            (short)*(unsigned int *)((long)signal_context + 0xe2b0);
                                        if (source != (unsigned int *)0x0)
                                            goto copy_source_diagonal;
                                        signal_halfword = 0x2a;
                                    }
                                    generated->source_diagonal = signal_halfword;
                                }
                                if (((capacity_or_endian_word != 0xe) &&
                                     (fixed_store_le16(output + 0x40,
                                                       generated->source_min_pq >> 8 |
                                                           generated->source_min_pq << 8),
                                      capacity_or_endian_word != 0x10)) &&
                                    (fixed_store_le16(output + 0x42,
                                                      generated->source_max_pq >> 8 |
                                                          generated->source_max_pq << 8),
                                     capacity_or_endian_word != 0x12)) {
                                    cursor->count = output + 0x46;
                                    cursor->records = output + 0x47;
                                    cursor->remaining = (unsigned long)(capacity - 0x47);
                                    fixed_store_le16(output + 0x44,
                                                     generated->source_diagonal >> 8 |
                                                         generated->source_diagonal << 8);
                                    *(unsigned char *)((long)context + 0xbc4) = 0;
                                    return 1;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}

struct metadata_output_state {
    void *context;
    void *signal_context;
    unsigned int *signal_words;
    unsigned int *source;
    const struct dm_control *control;
    struct dm_metadata_format *generated;
    struct metadata_output_cursor cursor;
    unsigned int measured;
    unsigned int exclusions;
    unsigned int output_mode;
    unsigned int format;
    int mode;
    int target_version;
    int signal_version;
    _Bool synthetic_header;
    _Bool missing_source;
};

static int write_legacy_metadata_extensions(struct metadata_output_state *state,
                                            struct dm_output_workspace *work)
{
    _Bool header_or_trim_test = state->synthetic_header;
    _Bool source_missing = state->missing_source;
    _Bool level3_required;
    int target_revision = state->target_version;
    int source_version_or_capacity = state->signal_version;
    int capacity_or_trim_index = (int)state->cursor.remaining;
    int mode = state->mode;
    long coefficient_index;
    long trim_index;
    struct dm_metadata_format *generated = state->generated;
    unsigned int *destination = (void *)state->generated;
    unsigned char *record_cursor;
    unsigned char *output_cursor = state->cursor.records;
    unsigned int *input_signal_words = state->signal_words;
    unsigned int *source = state->source;
    const unsigned char *source_00;
    unsigned int capacity = state->measured;
    unsigned int format = state->format;
    unsigned int exclusion_mask = state->exclusions;
    unsigned int average_or_field_count;
    unsigned int extension_value;
    unsigned int minimum_or_size_word;
    unsigned int average_pq;
    unsigned int original_minimum_pq;
    unsigned int maximum_average_pq;
    unsigned int adjusted_average_pq;
    unsigned int metadata_output_mode = state->output_mode;
    unsigned long remaining_capacity = state->cursor.remaining;
    unsigned short average_or_coefficient;
    unsigned short original_maximum_pq;
    void *context = state->context;
    void *selected_input_state = state->signal_context;
    if (header_or_trim_test || source_missing) {
        extension_value = *destination | 1;
    } else {
        extension_value = *source & 1;
    }
    header_or_trim_test = metadata_output_mode != 1;
    *destination = extension_value;
    source_missing = source == (unsigned int *)0x0;
    if (!header_or_trim_test && !source_missing) {
        if (((*source | format) >> 1 & 1) != 0)
            goto write_level1;
        if ((exclusion_mask >> 2 & 1) == 0) {
            level3_required = 0;
            if ((*source >> 5 & 1) == 0)
                goto disable_level2_clamp;
        inspect_active_area:
            if ((*(short *)((long)source + 0x52) == 0) &&
                (((short)source[0x15] == 0 && (*(short *)((long)source + 0x56) == 0)))) {
                header_or_trim_test = (short)source[0x16] != 0;
            } else {
                header_or_trim_test = 1;
            }
            goto write_level2;
        }
    clear_level3_requirement:
        level3_required = 0;
    select_level3:
        if (((*source & 8) == 0 && !level3_required) || ((exclusion_mask >> 3 & 1) != 0))
            goto select_level4;
    write_level3:
        work->trim_count = 0;
        if (output_cursor == (unsigned char *)0x0) {
            capacity = capacity + 0xb;
            output_cursor = (unsigned char *)0x0;
        publish_level3_flags:
            extension_value = extension_value | 8;
            *destination = extension_value;
            goto select_level4;
        }
        if (!level3_required) {
            *(unsigned short *)((long)context + 0x98a) = *(unsigned short *)((long)source + 0x7e);
            *(short *)((long)context + 0x98c) = (short)source[0x20];
            *(unsigned short *)((long)context + 0x98e) = *(unsigned short *)((long)source + 0x82);
        }
        source_version_or_capacity = (int)remaining_capacity;
        if ((((3 < source_version_or_capacity) && (fixed_store_le32(output_cursor, 0x6000000), source_version_or_capacity != 4)) &&
             (*(unsigned char *)(output_cursor + 4) = 3, source_version_or_capacity - 7U < 0xfffffffe)) &&
            (fixed_store_le16((void *)((long)output_cursor + 5),
                              *(unsigned short *)((long)context + 0x98a) >> 8 |
                                  *(unsigned short *)((long)context + 0x98a) << 8),
             source_version_or_capacity - 9U < 0xfffffffe)) {
            remaining_capacity = (unsigned long)(source_version_or_capacity - 0xbU);
            fixed_store_le16((void *)((long)output_cursor + 7),
                             *(unsigned short *)((long)context + 0x98c) >> 8 |
                                 *(unsigned short *)((long)context + 0x98c) << 8);
            if (source_version_or_capacity - 0xbU < 0xfffffffe) {
                fixed_store_le16((void *)((long)output_cursor + 9),
                                 *(unsigned short *)((long)context + 0x98e) >> 8 |
                                     *(unsigned short *)((long)context + 0x98e) << 8);
                output_cursor = (unsigned char *)((long)output_cursor + 0xb);
                extension_value = generated->flags;
                *(char *)((long)context + 0xbc4) = *(char *)((long)context + 0xbc4) + '\x01';
                goto publish_level3_flags;
            }
        }
        return 0;
    }
    if ((format >> 1 & 1) != 0) {
    write_level1:
        if ((exclusion_mask >> 1 & 1) != 0) {
            level3_required = 0;
        select_level2:
            if ((exclusion_mask >> 2 & 1) == 0) {
                if ((!header_or_trim_test && !source_missing) && ((*source >> 5 & 1) != 0))
                    goto inspect_active_area;
            disable_level2_clamp:
                header_or_trim_test = 0;
                goto write_level2;
            }
            goto select_level3_source;
        }
        if (output_cursor == (unsigned char *)0x0) {
            level3_required = 0;
            capacity = capacity + 0xb;
        publish_level1_flags:
            extension_value = extension_value | 2;
            *destination = extension_value;
            goto select_level2;
        }
        if ((header_or_trim_test || source_missing) || (((unsigned char)*source >> 1 & 1) == 0)) {
            level3_required = 0;
            extension_value = *(unsigned int *)((long)selected_input_state + 0xe3e4);
            average_or_coefficient = (unsigned short)*(unsigned int *)((long)selected_input_state + 0xe3e8);
            minimum_or_size_word = *(unsigned int *)((long)selected_input_state + 0xe3e0);
        } else {
            original_maximum_pq = *(unsigned short *)((long)source + 0x7a);
            average_or_coefficient = (unsigned short)source[0x1f];
            original_minimum_pq = (unsigned int)(unsigned short)source[0x1e];
            average_or_field_count = (unsigned int)average_or_coefficient;
            extension_value = (unsigned int)original_maximum_pq;
            minimum_or_size_word = original_minimum_pq;
            if ((target_revision < 4) || (source_version_or_capacity < 4)) {
                level3_required = extension_value < 0x822;
                extension_value = (unsigned int)original_maximum_pq;
                if (level3_required) {
                    extension_value = 0x821;
                }
                if (0x265 < original_minimum_pq) {
                    minimum_or_size_word = 0x266;
                }
                average_pq = (unsigned int)average_or_coefficient;
                maximum_average_pq = average_pq;
                if (extension_value < average_or_field_count || extension_value == average_pq) {
                    maximum_average_pq = extension_value - 1;
                }
                adjusted_average_pq = minimum_or_size_word + 700;
                if (minimum_or_size_word + 700 <= average_pq) {
                    adjusted_average_pq = maximum_average_pq;
                }
                average_or_coefficient = (unsigned short)adjusted_average_pq;
                if (target_revision < 4) {
                    level3_required = 0;
                } else {
                    level3_required = 1;
                    *(short *)((long)context + 0x98a) = (short)(((original_minimum_pq - minimum_or_size_word) + 0x1000) >> 1);
                    *(short *)((long)context + 0x98c) = (short)(((original_maximum_pq - extension_value) + 0x1000) >> 1);
                    *(short *)((long)context + 0x98e) =
                        (short)(((average_or_field_count - (adjusted_average_pq & 0xffff)) + 0x1000) >> 1);
                }
            } else {
                level3_required = 0;
            }
        }
        source_version_or_capacity = (int)remaining_capacity;
        *(short *)((long)context + 0x984) = (short)minimum_or_size_word;
        *(short *)((long)context + 0x986) = (short)extension_value;
        *(unsigned short *)((long)context + 0x988) = average_or_coefficient;
        if ((((3 < source_version_or_capacity) && (fixed_store_le32(output_cursor, 0x6000000), source_version_or_capacity != 4)) &&
             (*(unsigned char *)(output_cursor + 4) = 1, source_version_or_capacity - 7U < 0xfffffffe)) &&
            (fixed_store_le16((void *)((long)output_cursor + 5),
                              *(unsigned short *)((long)context + 0x984) >> 8 |
                                  *(unsigned short *)((long)context + 0x984) << 8),
             source_version_or_capacity - 9U < 0xfffffffe)) {
            remaining_capacity = (unsigned long)(source_version_or_capacity - 0xbU);
            fixed_store_le16((void *)((long)output_cursor + 7),
                             *(unsigned short *)((long)context + 0x986) >> 8 |
                                 *(unsigned short *)((long)context + 0x986) << 8);
            if (source_version_or_capacity - 0xbU < 0xfffffffe) {
                fixed_store_le16((void *)((long)output_cursor + 9),
                                 *(unsigned short *)((long)context + 0x988) >> 8 |
                                     *(unsigned short *)((long)context + 0x988) << 8);
                output_cursor = (unsigned char *)((long)output_cursor + 0xb);
                extension_value = generated->flags;
                *(char *)((long)context + 0xbc4) = *(char *)((long)context + 0xbc4) + '\x01';
                goto publish_level1_flags;
            }
        }
        return 0;
    }
    if ((exclusion_mask >> 2 & 1) == 0) {
        level3_required = 0;
        header_or_trim_test = 0;
    write_level2:
        work->trim_count = 0;
        extension_value = source[0x23];
        if ((mode == 0) || ((int)extension_value < 2)) {
            initialize_trim_indices(work, extension_value);
        } else {
            dovi_dm_FindTrimIdxUsed((int *)(source + 0x21), (long)source, (long)input_signal_words,
                                    (long)context, (unsigned int *)&work->trims.pairs[0],
                                    (int *)&work->trim_count);
            extension_value = work->trim_count;
        }
        work->trim_count = extension_value;
        extension_value = source[0x22] * 2;
        if (output_cursor == (unsigned char *)0x0) {
            capacity = capacity + work->trim_count * (extension_value + 5);
        } else {
            work->level10.fields.parameters[0] = work->level10.fields.parameters[1] =
                work->level10.fields.parameters[2] = work->level10.fields.parameters[3] =
                    work->level10.fields.parameters[4] = work->level10.fields.parameters[5] = 0;
            work->level10.fields.target = 0;
            work->level10.fields.minimum = 0;
            work->level10.fields.maximum = 0;
            if (0 < (int)work->trim_count) {
                trim_index = 0;
                minimum_or_size_word = (extension_value & 0xff00ff00) >> 8 | (extension_value & 0xff00ff) << 8;
                do {
                    if (trim_index == 0x10)
                        dm_limit_trap();
                    source_version_or_capacity = (int)remaining_capacity;
                    if (source_version_or_capacity < 4)
                        return 0;
                    capacity_or_trim_index = *(int *)((long)&work->trims.pairs[0] + trim_index * 4);
                    fixed_store_le32(output_cursor, minimum_or_size_word >> 0x10 | minimum_or_size_word << 0x10);
                    if (source_version_or_capacity == 4)
                        return 0;
                    *(unsigned char *)(output_cursor + 4) = 2;
                    remaining_capacity = (unsigned long)(source_version_or_capacity - 5U);
                    average_or_field_count = source[0x22];
                    if ((int)(source_version_or_capacity - 5U) < (int)(average_or_field_count * 2))
                        return 0;
                    source_00 = (const unsigned char *)((long)source + (long)capacity_or_trim_index * 0xe + 0x90);
                    if (header_or_trim_test) {
                        dovi_MemCpyByte((void *)(&work->level10.fields.target), (const void *)source_00,
                                   (int)extension_value);
                        if (0x7ff < work->level10.fields.minimum) {
                            work->level10.fields.minimum = 0x800;
                        }
                        average_or_field_count = source[0x22];
                        source_00 = (const unsigned char *)&work->level10.fields.target;
                    }
                    output_cursor = (unsigned char *)((long)output_cursor + 5);
                    if (0 < (int)average_or_field_count) {
                        coefficient_index = 0;
                        record_cursor = output_cursor;
                        do {
                            source_version_or_capacity = (int)remaining_capacity;
                            remaining_capacity = (unsigned long)(source_version_or_capacity - 2);
                            if (source_version_or_capacity < 2)
                                return 0;
                            average_or_coefficient = fixed_load_le16(source_00 + coefficient_index * 2);
                            coefficient_index = coefficient_index + 1;
                            output_cursor = (unsigned char *)((long)record_cursor + 2);
                            fixed_store_le16(record_cursor, average_or_coefficient >> 8 | average_or_coefficient << 8);
                            record_cursor = output_cursor;
                        } while (coefficient_index < (int)source[0x22]);
                    }
                    trim_index = trim_index + 1;
                    *(char *)((long)context + 0xbc4) = *(char *)((long)context + 0xbc4) + '\x01';
                } while (trim_index < (int)work->trim_count);
            }
        }
        extension_value = *destination | 4;
        *destination = extension_value;
    select_level3_source:
        if (source != (unsigned int *)0x0)
            goto select_level3;
        if ((level3_required) && (level3_required = 1, (exclusion_mask >> 3 & 1) == 0))
            goto write_level3;
    } else if (source != (unsigned int *)0x0)
        goto clear_level3_requirement;
select_level4:
    work->trim_count = 0;
    if ((exclusion_mask >> 4 & 1) == 0) {
        if (output_cursor == (unsigned char *)0x0) {
            capacity = capacity + 9;
        publish_level4_flags:
            *destination = extension_value | 0x10;
            goto finish_legacy;
        }
        source_version_or_capacity = (int)remaining_capacity;
        *(short *)((long)context + 0xbc0) = (short)source[0xad];
        *(unsigned short *)((long)context + 0xbc2) = *(unsigned short *)((long)source + 0x2b6);
        if (((3 < source_version_or_capacity) && (fixed_store_le32(output_cursor, 0x4000000), source_version_or_capacity != 4)) &&
            (*(unsigned char *)(output_cursor + 4) = 4, source_version_or_capacity - 7U < 0xfffffffe)) {
            remaining_capacity = (unsigned long)(source_version_or_capacity - 9U);
            fixed_store_le16((void *)((long)output_cursor + 5),
                             *(unsigned short *)((long)context + 0xbc0) >> 8 |
                                 *(unsigned short *)((long)context + 0xbc0) << 8);
            if (source_version_or_capacity - 9U < 0xfffffffe) {
                fixed_store_le16((void *)((long)output_cursor + 7),
                                 *(unsigned short *)((long)context + 0xbc2) >> 8 |
                                     *(unsigned short *)((long)context + 0xbc2) << 8);
                output_cursor = (unsigned char *)((long)output_cursor + 9);
                extension_value = generated->flags;
                *(char *)((long)context + 0xbc4) = *(char *)((long)context + 0xbc4) + '\x01';
                goto publish_level4_flags;
            }
        }
        return 0;
    }

finish_legacy:
    state->measured = capacity;
    state->cursor.records = output_cursor;
    state->cursor.remaining = remaining_capacity;
    return 1;
}

static int write_level8_metadata(struct metadata_output_state *state,
                                 struct dm_output_workspace *work)
{
    int capacity_checkpoint = state->signal_version;
    int unused_capacity_status = (int)state->cursor.remaining;
    int mode = state->mode;
    long trim_index;
    long selected_trim_index;
    unsigned char *unknown_tail_source;
    unsigned int *destination = (void *)state->generated;
    unsigned char *record_cursor;
    unsigned char *output_cursor = state->cursor.records;
    unsigned int *input_signal_words = state->signal_words;
    unsigned int *source = state->source;
    unsigned int capacity = state->measured;
    unsigned int exclusion_mask = state->exclusions;
    unsigned int capacity_or_endian_word;
    unsigned int trim_index_or_count;
    unsigned int payload_remainder;
    unsigned int optional_field_groups;
    unsigned int metadata_output_mode = state->output_mode;
    unsigned long optional_byte_index;
    unsigned long remaining_capacity = state->cursor.remaining;
    unsigned long optional_or_tail_offset;
    unsigned char *optional_word_output;
    unsigned short coefficient_value;
    void *context = state->context;
    work->trim_count = 0;
    if ((exclusion_mask >> 8 & 1) == 0) {
        trim_index_or_count = source[0x46];
        if ((mode == 0) || ((int)trim_index_or_count < 2)) {
            initialize_trim_indices(work, trim_index_or_count);
        } else {
            dovi_dm_FindTrimIdxUsed((int *)(source + 0x44), (long)source, (long)input_signal_words,
                                    (long)context, (unsigned int *)&work->trims.pairs[0],
                                    (int *)&work->trim_count);
            trim_index_or_count = work->trim_count;
        }
        work->trim_count = trim_index_or_count;
        if (0 < (int)work->trim_count) {
            selected_trim_index = 0;
            do {
                if (selected_trim_index == 0x10)
                    dm_limit_trap();
                trim_index_or_count = *(unsigned int *)((long)&work->trims.pairs[0] + selected_trim_index * 4);
                trim_index = (long)(int)trim_index_or_count;
                if (4 < trim_index_or_count)
                    dm_limit_trap();
                payload_remainder = *(unsigned int *)((long)context + trim_index * 4 + 0x42f8);
                if (metadata_output_mode != 1) {
                    payload_remainder = payload_remainder - *(int *)((long)context + trim_index * 4 + 0x430c);
                }
                if (output_cursor == (unsigned char *)0x0) {
                    capacity = capacity + payload_remainder + 5;
                } else {
                    capacity_checkpoint = (int)remaining_capacity;
                    if (capacity_checkpoint < 4)
                        return 0;
                    capacity_or_endian_word = (payload_remainder & 0xff00ff00) >> 8 | (payload_remainder & 0xff00ff) << 8;
                    unused_capacity_status = -1;
                    fixed_store_le32(output_cursor, capacity_or_endian_word >> 0x10 | capacity_or_endian_word << 0x10);
                    if (((capacity_checkpoint == 4) || (*(unsigned char *)(output_cursor + 4) = 8, payload_remainder == 0)) ||
                        (capacity_checkpoint == 5))
                        return 0;
                    capacity_or_endian_word = capacity_checkpoint - 0x12;
                    *(unsigned char *)((long)output_cursor + 5) =
                        *(unsigned char *)((long)source + (long)(int)trim_index_or_count * 0x2a + 0x11c);
                    if (0xfffffff3 < capacity_or_endian_word)
                        return 0;
                    coefficient_value = *(unsigned short *)((long)source + (long)(int)trim_index_or_count * 0x2a + 0x11e);
                    payload_remainder = payload_remainder - 0xd;
                    fixed_store_le16((void *)((long)output_cursor + 6), coefficient_value >> 8 | coefficient_value << 8);
                    coefficient_value = *(unsigned short *)((long)source + (long)(int)trim_index_or_count * 0x2a + 0x120);
                    fixed_store_le16(output_cursor + 8, coefficient_value >> 8 | coefficient_value << 8);
                    coefficient_value = *(unsigned short *)((long)source + (long)(int)trim_index_or_count * 0x2a + 0x122);
                    fixed_store_le16((void *)((long)output_cursor + 10), coefficient_value >> 8 | coefficient_value << 8);
                    coefficient_value = *(unsigned short *)((long)source + (long)(int)trim_index_or_count * 0x2a + 0x124);
                    fixed_store_le16(output_cursor + 12, coefficient_value >> 8 | coefficient_value << 8);
                    coefficient_value = *(unsigned short *)((long)source + (long)(int)trim_index_or_count * 0x2a + 0x126);
                    fixed_store_le16((void *)((long)output_cursor + 0xe), coefficient_value >> 8 | coefficient_value << 8);
                    coefficient_value = *(unsigned short *)((long)source + (long)(int)trim_index_or_count * 0x2a + 0x128);
                    optional_field_groups = payload_remainder >> 1;
                    if (5 < payload_remainder) {
                        optional_field_groups = 2;
                    }
                    record_cursor = (unsigned char *)((long)output_cursor + 0x12);
                    fixed_store_le16(output_cursor + 16, coefficient_value >> 8 | coefficient_value << 8);
                    if (optional_field_groups == 0) {
                        remaining_capacity = (unsigned long)capacity_or_endian_word;
                        output_cursor = record_cursor;
                    } else {
                        optional_or_tail_offset = 0;
                        do {
                            if ((capacity_or_endian_word & 0xfffffffe) == optional_or_tail_offset)
                                return 0;
                            coefficient_value = *(unsigned short *)((long)source + optional_or_tail_offset +
                                                         (long)(int)trim_index_or_count * 0x2a + 0x12a);
                            optional_word_output = (unsigned char *)((long)record_cursor + optional_or_tail_offset);
                            optional_or_tail_offset = optional_or_tail_offset + 2;
                            fixed_store_le16(optional_word_output, coefficient_value >> 8 | coefficient_value << 8);
                        } while ((unsigned long)optional_field_groups << 1 != optional_or_tail_offset);
                        payload_remainder = payload_remainder + optional_field_groups * -2;
                        output_cursor = (unsigned char *)((long)record_cursor + optional_or_tail_offset);
                        if (optional_field_groups == 2) {
                            optional_field_groups = (payload_remainder & 0xff) / 6;
                            if (0x11 < payload_remainder) {
                                optional_field_groups = 2;
                            }
                            if (optional_field_groups != 0) {
                                optional_byte_index = 0;
                                remaining_capacity = (capacity_or_endian_word - optional_or_tail_offset) & 0xffffffff;
                                do {
                                    if ((0x12 - capacity_checkpoint) + (int)optional_or_tail_offset + (int)optional_byte_index == 0)
                                        return 0;
                                    remaining_capacity = (unsigned long)((int)remaining_capacity - 1);
                                    *(char *)((long)record_cursor + optional_byte_index + optional_or_tail_offset) =
                                        (char)*(unsigned short *)((long)source + optional_byte_index * 2 +
                                                                  (long)(int)trim_index_or_count * 0x2a + 0x12e);
                                    optional_byte_index = optional_byte_index + 1;
                                } while (optional_field_groups * 6 != optional_byte_index);
                                payload_remainder = payload_remainder + optional_field_groups * -6;
                                output_cursor = (unsigned char *)((long)record_cursor + optional_byte_index + optional_or_tail_offset);
                                if (optional_field_groups == 2) {
                                    if ((metadata_output_mode == 1) || (payload_remainder != 0)) {
                                        if ((metadata_output_mode != 1) ||
                                            (payload_remainder != *(unsigned int *)((long)context +
                                                                         trim_index * 4 + 0x430c))) {
                                            _printk(
                                                dovi_dm_assertion_format,
                                                "../dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmCfgFxp.c",
                                                "GenerateOutputMd", 0x7d8,
                                                "(!outputUnkFld && !extLen) || (outputUnkFld && (int)extLen == hDm->trim8UnkFldLens[trimIdx])");
                                            dump_stack();
                                        }
                                        if (0 < (int)payload_remainder) {
                                            optional_or_tail_offset = 0;
                                            record_cursor = output_cursor;
                                            do {
                                                if ((int)remaining_capacity == 0)
                                                    return 0;
                                                remaining_capacity = (unsigned long)((int)remaining_capacity - 1);
                                                unknown_tail_source =
                                                    (unsigned char *)(*(long *)((long)context +
                                                                                trim_index * 8 +
                                                                                0x4320) +
                                                                      optional_or_tail_offset);
                                                optional_or_tail_offset = optional_or_tail_offset + 1;
                                                output_cursor = (unsigned char *)((long)record_cursor + 1);
                                                *(unsigned char *)record_cursor = *unknown_tail_source;
                                                record_cursor = output_cursor;
                                            } while (payload_remainder != optional_or_tail_offset);
                                        }
                                    }
                                    *(char *)((long)context + 0xbc4) =
                                        *(char *)((long)context + 0xbc4) + '\x01';
                                    goto next_trim_record;
                                }
                                goto finish_trim_record;
                            }
                        }
                        remaining_capacity = (capacity_or_endian_word - optional_or_tail_offset) & 0xffffffff;
                    }
                finish_trim_record:
                    *(char *)((long)context + 0xbc4) = *(char *)((long)context + 0xbc4) + '\x01';
                    if (payload_remainder != 0) {
                        _printk(
                            dovi_dm_assertion_format,
                            "../dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmCfgFxp.c",
                            "GenerateOutputMd", 0x7e3, "extLen == 0");
                        dump_stack();
                    }
                }
            next_trim_record:
                selected_trim_index = selected_trim_index + 1;
                *destination = *destination | 0x100;
            } while (selected_trim_index < (int)work->trim_count);
        }
    }

    state->measured = capacity;
    state->cursor.records = output_cursor;
    state->cursor.remaining = remaining_capacity;
    return 1;
}

static int write_primaries_metadata(struct metadata_output_state *state,
                                    struct dm_output_workspace *work)
{
    int capacity_checkpoint = state->signal_version;
    long tail_copy_offset;
    struct dm_metadata_format *generated = state->generated;
    unsigned int *destination = (void *)state->generated;
    unsigned char *output_cursor = state->cursor.records;
    unsigned int *source = state->source;
    unsigned int capacity = state->measured;
    unsigned int primaries_record_word;
    unsigned int metadata_output_mode = state->output_mode;
    unsigned long remaining_capacity = state->cursor.remaining;
    void *context = state->context;

    if ((state->exclusions >> 9 & 1) != 0)
        return 1;
    if (metadata_output_mode == 0) {
        primaries_record_word = *(int *)((long)context + 200) - 0xf;
        generated->primaries_index = 0xff;
        *(short *)((long)context + 0x968) =
            (short)(*(int *)((long)context + 0xa8) >> (primaries_record_word & 0x1f));
        *(short *)((long)context + 0x96a) =
            (short)(*(int *)((long)context + 0xac) >> (primaries_record_word & 0x1f));
        *(short *)((long)context + 0x96c) =
            (short)(*(int *)((long)context + 0xb0) >> (primaries_record_word & 0x1f));
        *(short *)((long)context + 0x96e) =
            (short)(*(int *)((long)context + 0xb4) >> (primaries_record_word & 0x1f));
        *(short *)((long)context + 0x970) =
            (short)(*(int *)((long)context + 0xb8) >> (primaries_record_word & 0x1f));
        *(short *)((long)context + 0x972) =
            (short)(*(int *)((long)context + 0xbc) >> (primaries_record_word & 0x1f));
        *(short *)((long)context + 0x974) =
            (short)(*(int *)((long)context + 0xc0) >> (primaries_record_word & 0x1f));
        *(short *)((long)context + 0x976) =
            (short)(*(int *)((long)context + 0xc4) >> (primaries_record_word & 0x1f));
    select_custom_primaries_size:
        primaries_record_word = 0x11;
    } else {
        if ((source == (unsigned int *)0x0) ||
            ((*(unsigned char *)((long)source + 1) >> 1 & 1) == 0))
            goto finish_primaries;
        generated->primaries_index = *(unsigned char *)((long)source + 0x5a);
        dovi_MemCpyByte((void *)((long)context + 0x968), (const void *)(source + 0x17), (int)0x10);
        if (0x12 < generated->primaries_index)
            goto select_custom_primaries_size;
        primaries_record_word = 1;
    }
    if (metadata_output_mode == 1) {
        primaries_record_word = *(int *)((long)context + 0x371c) + primaries_record_word;
    }
    if (output_cursor == (unsigned char *)0x0) {
        capacity = capacity + primaries_record_word + 5;
    publish_primaries_flags:
        *destination = *destination | 0x200;
        goto finish_primaries;
    }
    capacity_checkpoint = (int)remaining_capacity;
    if (((3 < capacity_checkpoint) &&
         (primaries_record_word = (primaries_record_word & 0xff00ff00) >> 8 | (primaries_record_word & 0xff00ff) << 8,
          fixed_store_le32(output_cursor, primaries_record_word >> 0x10 | primaries_record_word << 0x10), capacity_checkpoint != 4)) &&
        (*(unsigned char *)(output_cursor + 4) = 9, capacity_checkpoint != 5)) {
        *(unsigned char *)((long)output_cursor + 5) = generated->primaries_index;
        primaries_record_word = capacity_checkpoint - 6;
        if (generated->primaries_index < 0x13) {
            output_cursor = (unsigned char *)((long)output_cursor + 6);
        } else {
            if (primaries_record_word < 0x10)
                return 0;
            primaries_record_word = capacity_checkpoint - 0x16;
            fixed_store_le16((void *)((long)output_cursor + 6),
                             generated->primaries[0] >> 8 | generated->primaries[0] << 8);
            fixed_store_le16(output_cursor + 8,
                             generated->primaries[1] >> 8 | generated->primaries[1] << 8);
            fixed_store_le16((void *)((long)output_cursor + 10),
                             generated->primaries[2] >> 8 | generated->primaries[2] << 8);
            fixed_store_le16(output_cursor + 12,
                             generated->primaries[3] >> 8 | generated->primaries[3] << 8);
            fixed_store_le16((void *)((long)output_cursor + 0xe),
                             generated->primaries[4] >> 8 | generated->primaries[4] << 8);
            fixed_store_le16(output_cursor + 16,
                             generated->primaries[5] >> 8 | generated->primaries[5] << 8);
            fixed_store_le16((void *)((long)output_cursor + 0x12),
                             generated->primaries[6] >> 8 | generated->primaries[6] << 8);
            fixed_store_le16(output_cursor + 20,
                             generated->primaries[7] >> 8 | generated->primaries[7] << 8);
            output_cursor = (unsigned char *)((long)output_cursor + 0x16);
        }
        remaining_capacity = (unsigned long)primaries_record_word;
        if ((metadata_output_mode == 1) && (0 < *(int *)((long)context + 0x371c))) {
            tail_copy_offset = 0;
            do {
                if (primaries_record_word == (unsigned int)tail_copy_offset)
                    return 0;
                *(unsigned char *)((long)output_cursor + tail_copy_offset) =
                    *(unsigned char *)(*(long *)((long)context + 0x3b40) + tail_copy_offset);
                tail_copy_offset = tail_copy_offset + 1;
            } while (tail_copy_offset < *(int *)((long)context + 0x371c));
            remaining_capacity = (unsigned long)(primaries_record_word - (int)tail_copy_offset);
            output_cursor = (unsigned char *)((long)output_cursor + tail_copy_offset);
        }
        *(char *)((long)context + 0xbc4) = *(char *)((long)context + 0xbc4) + '\x01';
        goto publish_primaries_flags;
    }
    return 0;

finish_primaries:

    state->measured = capacity;
    state->cursor.records = output_cursor;
    state->cursor.remaining = remaining_capacity;
    return 1;
}

static int write_level10_metadata(struct metadata_output_state *state,
                                  struct dm_output_workspace *work)
{
    int capacity_checkpoint = state->signal_version;
    int unused_capacity_status = (int)state->cursor.remaining;
    int mode = state->mode;
    short target_display_id;
    unsigned int *destination = (void *)state->generated;
    unsigned char *output_cursor = state->cursor.records;
    unsigned int *source = state->source;
    unsigned int capacity = state->measured;
    unsigned int exclusion_mask = state->exclusions;
    unsigned int record_count_or_size;
    unsigned int capacity_or_endian_word;
    unsigned int metadata_output_mode = state->output_mode;
    unsigned long tail_copy_offset;
    unsigned long remaining_capacity = state->cursor.remaining;
    unsigned long trim_index;
    unsigned short coefficient_value;
    void *context = state->context;
    if (((exclusion_mask >> 10 & 1) == 0) && (record_count_or_size = source[0x88], 0 < (int)record_count_or_size)) {
        trim_index = 0;
        do {
            if (mode == 0) {
            serialize_level10:
                if (4 < trim_index)
                    dm_limit_trap();
                record_count_or_size = *(unsigned int *)((long)context + trim_index * 4 + 0x4348);
                if (metadata_output_mode != 1) {
                    record_count_or_size = record_count_or_size - *(int *)((long)context + trim_index * 4 + 0x435c);
                }
                if (output_cursor == (unsigned char *)0x0) {
                    capacity = capacity + record_count_or_size + 5;
                } else {
                    capacity_checkpoint = (int)remaining_capacity;
                    if (((capacity_checkpoint < 4) ||
                         (capacity_or_endian_word = (record_count_or_size & 0xff00ff00) >> 8 | (record_count_or_size & 0xff00ff) << 8,
                          fixed_store_le32(output_cursor, capacity_or_endian_word >> 0x10 | capacity_or_endian_word << 0x10),
                          capacity_checkpoint == 4)) ||
                        (*(unsigned char *)(output_cursor + 4) = 10, capacity_checkpoint == 5))
                        return 0;
                    capacity_or_endian_word = capacity_checkpoint - 0xb;
                    *(char *)((long)output_cursor + 5) = (char)source[trim_index * 6 + 0x89];
                    if (0xfffffffa < capacity_or_endian_word)
                        return 0;
                    coefficient_value = *(unsigned short *)((long)source + trim_index * 0x18 + 0x226);
                    fixed_store_le16((void *)((long)output_cursor + 6), coefficient_value >> 8 | coefficient_value << 8);
                    fixed_store_le16(output_cursor + 8,
                                     (unsigned short)source[trim_index * 6 + 0x8a] >> 8 |
                                         (unsigned short)source[trim_index * 6 + 0x8a] << 8);
                    *(unsigned char *)((long)output_cursor + 10) =
                        *(unsigned char *)((long)source + trim_index * 0x18 + 0x22a);
                    if (*(unsigned short *)((long)source + trim_index * 0x18 + 0x22a) < 0x13) {
                        output_cursor = (unsigned char *)((long)output_cursor + 0xb);
                        record_count_or_size = record_count_or_size - 6;
                    } else {
                        if (capacity_or_endian_word < 0x10)
                            return 0;
                        capacity_or_endian_word = capacity_checkpoint - 0x1b;
                        record_count_or_size = record_count_or_size - 0x16;
                        fixed_store_le16((void *)((long)output_cursor + 0xb),
                                         (unsigned short)source[trim_index * 6 + 0x8b] >> 8 |
                                             (unsigned short)source[trim_index * 6 + 0x8b] << 8);
                        coefficient_value = *(unsigned short *)((long)source + trim_index * 0x18 + 0x22e);
                        fixed_store_le16((void *)((long)output_cursor + 0xd), coefficient_value >> 8 | coefficient_value << 8);
                        fixed_store_le16((void *)((long)output_cursor + 0xf),
                                         (unsigned short)source[trim_index * 6 + 0x8c] >> 8 |
                                             (unsigned short)source[trim_index * 6 + 0x8c] << 8);
                        coefficient_value = *(unsigned short *)((long)source + trim_index * 0x18 + 0x232);
                        fixed_store_le16((void *)((long)output_cursor + 0x11), coefficient_value >> 8 | coefficient_value << 8);
                        fixed_store_le16((void *)((long)output_cursor + 0x13),
                                         (unsigned short)source[trim_index * 6 + 0x8d] >> 8 |
                                             (unsigned short)source[trim_index * 6 + 0x8d] << 8);
                        coefficient_value = *(unsigned short *)((long)source + trim_index * 0x18 + 0x236);
                        fixed_store_le16((void *)((long)output_cursor + 0x15), coefficient_value >> 8 | coefficient_value << 8);
                        fixed_store_le16((void *)((long)output_cursor + 0x17),
                                         (unsigned short)source[trim_index * 6 + 0x8e] >> 8 |
                                             (unsigned short)source[trim_index * 6 + 0x8e] << 8);
                        coefficient_value = *(unsigned short *)((long)source + trim_index * 0x18 + 0x23a);
                        fixed_store_le16((void *)((long)output_cursor + 0x19), coefficient_value >> 8 | coefficient_value << 8);
                        output_cursor = (unsigned char *)((long)output_cursor + 0x1b);
                    }
                    remaining_capacity = (unsigned long)capacity_or_endian_word;
                    if ((metadata_output_mode == 1) || (record_count_or_size != 0)) {
                        if ((metadata_output_mode != 1) ||
                            (record_count_or_size != *(unsigned int *)((long)context + trim_index * 4 + 0x435c))) {
                            _printk(
                                dovi_dm_assertion_format,
                                "../dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmCfgFxp.c",
                                "GenerateOutputMd", 0x841,
                                "(!outputUnkFld && !extLen) || (outputUnkFld && (int)extLen == hDm->trim10UnkFldLens[trimIdx])");
                            dump_stack();
                        }
                        if (0 < (int)record_count_or_size) {
                            tail_copy_offset = 0;
                            do {
                                if (capacity_or_endian_word == (unsigned int)tail_copy_offset)
                                    return 0;
                                *(unsigned char *)((long)output_cursor + tail_copy_offset) =
                                    *(unsigned char *)(*(long *)((long)context + trim_index * 8 +
                                                                 0x4370) +
                                                       tail_copy_offset);
                                tail_copy_offset = tail_copy_offset + 1;
                            } while (record_count_or_size != tail_copy_offset);
                            remaining_capacity = (unsigned long)(capacity_or_endian_word - (int)tail_copy_offset);
                            output_cursor = (unsigned char *)((long)output_cursor + tail_copy_offset);
                        }
                    }
                    *(char *)((long)context + 0xbc4) = *(char *)((long)context + 0xbc4) + '\x01';
                }
                *destination = *destination | 0x400;
                record_count_or_size = source[0x88];
            } else if (0 < (int)work->trim_count) {
                target_display_id = (short)source[trim_index * 6 + 0x89];
                if (*(short *)((long)source + (long)(int)(unsigned int)work->trims.pairs[0] * 0x2a +
                               0x11c) == target_display_id)
                    goto serialize_level10;
                if (work->trim_count != 1) {
                    if (*(short *)((long)source +
                                   (long)(int)(*(
                                       unsigned int *)((unsigned char *)&work->trims.pairs[0] +
                                                       4)) *
                                       0x2a +
                                   0x11c) == target_display_id)
                        goto serialize_level10;
                    if (work->trim_count != 2) {
                        if (*(short *)((long)source + (long)(int)work->trims.pairs[1] * 0x2a +
                                       0x11c) == target_display_id)
                            goto serialize_level10;
                        if (work->trim_count != 3) {
                            if (*(short *)((long)source +
                                           (long)(int)(*(unsigned int *)((unsigned char *)&work
                                                                             ->trims.pairs[1] +
                                                                         4)) *
                                               0x2a +
                                           0x11c) == target_display_id)
                                goto serialize_level10;
                            if (work->trim_count != 4) {
                                if (*(short *)((long)source +
                                               (long)(int)work->trims.pairs[2] * 0x2a + 0x11c) ==
                                    target_display_id)
                                    goto serialize_level10;
                                if (work->trim_count != 5) {
                                    if (*(short *)((long)source +
                                                   (long)(int)(*(
                                                       unsigned int *)((unsigned char *)&work->trims
                                                                           .pairs[2] +
                                                                       4)) *
                                                       0x2a +
                                                   0x11c) == target_display_id)
                                        goto serialize_level10;
                                    if (work->trim_count != 6) {
                                        if (*(short *)((long)source +
                                                       (long)(int)work->trims.pairs[3] * 0x2a +
                                                       0x11c) == target_display_id)
                                            goto serialize_level10;
                                        if (work->trim_count != 7) {
                                            if (*(short
                                                      *)((long)source +
                                                         (long)(int)(*(
                                                             unsigned int *)((unsigned char *)&work
                                                                                 ->trims.pairs[3] +
                                                                             4)) *
                                                             0x2a +
                                                         0x11c) == target_display_id)
                                                goto serialize_level10;
                                            if (work->trim_count != 8) {
                                                if (*(short *)((long)source +
                                                               (long)(int)work->trims.pairs[4] *
                                                                   0x2a +
                                                               0x11c) == target_display_id)
                                                    goto serialize_level10;
                                                if (work->trim_count != 9) {
                                                    if (*(short *)((long)source +
                                                                   (long)(int)(*(
                                                                       unsigned int
                                                                           *)((unsigned char *)&work
                                                                                  ->trims.pairs[4] +
                                                                              4)) *
                                                                       0x2a +
                                                                   0x11c) == target_display_id)
                                                        goto serialize_level10;
                                                    if (work->trim_count != 10) {
                                                        if (*(short *)((long)source +
                                                                       (long)(int)work->trims
                                                                               .pairs[5] *
                                                                           0x2a +
                                                                       0x11c) == target_display_id)
                                                            goto serialize_level10;
                                                        if (work->trim_count != 0xb) {
                                                            if (*(short
                                                                      *)((long)source +
                                                                         (long)(int)(*(
                                                                             unsigned int
                                                                                 *)((unsigned char
                                                                                         *)&work
                                                                                        ->trims
                                                                                        .pairs[5] +
                                                                                    4)) *
                                                                             0x2a +
                                                                         0x11c) == target_display_id)
                                                                goto serialize_level10;
                                                            if (work->trim_count != 0xc) {
                                                                if (*(short *)((long)source +
                                                                               (long)(int)work
                                                                                       ->trims
                                                                                       .pairs[6] *
                                                                                   0x2a +
                                                                               0x11c) == target_display_id)
                                                                    goto serialize_level10;
                                                                if (work->trim_count != 0xd) {
                                                                    if (*(short
                                                                              *)((long)source +
                                                                                 (long)(int)(*(
                                                                                     unsigned int
                                                                                         *)((unsigned char
                                                                                                 *)&work
                                                                                                ->trims
                                                                                                .pairs
                                                                                                    [6] +
                                                                                            4)) *
                                                                                     0x2a +
                                                                                 0x11c) == target_display_id)
                                                                        goto serialize_level10;
                                                                    if (work->trim_count != 0xe) {
                                                                        if (*(short
                                                                                  *)((long)source +
                                                                                     (long)(int)work
                                                                                             ->trims
                                                                                             .pairs
                                                                                                 [7] *
                                                                                         0x2a +
                                                                                     0x11c) ==
                                                                            target_display_id)
                                                                            goto serialize_level10;
                                                                        if (work->trim_count !=
                                                                            0xf) {
                                                                            if (*(short
                                                                                      *)((long)
                                                                                             source +
                                                                                         (long)(int)(*(
                                                                                             unsigned int
                                                                                                 *)((unsigned char
                                                                                                         *)&work
                                                                                                        ->trims
                                                                                                        .pairs
                                                                                                            [7] +
                                                                                                    4)) *
                                                                                             0x2a +
                                                                                         0x11c) ==
                                                                                target_display_id)
                                                                                goto serialize_level10;
                                                                            if (work->trim_count !=
                                                                                0x10) {
                                                                                dm_limit_trap();
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            unused_capacity_status = (int)remaining_capacity;
            trim_index = trim_index + 1;
        } while ((long)trim_index < (long)(int)record_count_or_size);
    }
    state->measured = capacity;
    state->cursor.records = output_cursor;
    state->cursor.remaining = remaining_capacity;
    return 1;
}

static int finish_output_metadata(struct metadata_output_state *state,
                                  struct dm_output_workspace *work)
{
    const struct dm_control *control = state->control;
    int revision_or_unknown_size = state->target_version;
    int capacity_or_output_size = (int)state->cursor.remaining;
    int mode = state->mode;
    long tail_copy_offset;
    struct dm_metadata_format *generated = state->generated;
    unsigned char *level11_source_byte;
    unsigned int *destination = (void *)state->generated;
    unsigned char *record_cursor;
    unsigned char *output_cursor = state->cursor.records;
    unsigned int *source = state->source;
    unsigned int capacity = state->measured;
    unsigned int exclusion_or_record_word = state->exclusions;
    unsigned int level11_payload_size;
    unsigned int swapped_payload_length;
    unsigned int metadata_output_mode = state->output_mode;
    unsigned long payload_bytes_remaining = state->cursor.remaining;
    void *context = state->context;

    if ((exclusion_or_record_word >> 0xb & 1) == 0) {
        prepare_level11_metadata(source, context);
        level11_payload_size = *(unsigned int *)((long)context + 0x4398);
        if (metadata_output_mode == 1) {
            level11_payload_size = *(int *)((long)context + 0x3724) + level11_payload_size;
        }
        if (output_cursor == (unsigned char *)0x0) {
            record_cursor = (unsigned char *)0x0;
            capacity = capacity + level11_payload_size + 5;
        } else {
            if ((capacity_or_output_size < 4) ||
                (swapped_payload_length = (level11_payload_size & 0xff00ff00) >> 8 | (level11_payload_size & 0xff00ff) << 8,
                 fixed_store_le32(output_cursor, swapped_payload_length >> 0x10 | swapped_payload_length << 0x10), capacity_or_output_size == 4))
                return -1;
            record_cursor = (unsigned char *)((long)output_cursor + 5);
            capacity_or_output_size = capacity_or_output_size + -5;
            *(unsigned char *)(output_cursor + 4) = 0xb;
            if (0 < (int)level11_payload_size) {
                payload_bytes_remaining = (unsigned long)level11_payload_size;
                output_cursor = record_cursor;
                level11_source_byte = (unsigned char *)((long)context + 0x439c);
                do {
                    if (capacity_or_output_size == 0)
                        return -1;
                    capacity_or_output_size = capacity_or_output_size + -1;
                    payload_bytes_remaining = payload_bytes_remaining - 1;
                    record_cursor = (unsigned char *)((long)output_cursor + 1);
                    *(unsigned char *)output_cursor = *level11_source_byte;
                    output_cursor = record_cursor;
                    level11_source_byte = level11_source_byte + 1;
                } while (payload_bytes_remaining != 0);
            }
            if ((metadata_output_mode == 1) && (0 < *(int *)((long)context + 0x3724))) {
                tail_copy_offset = 0;
                do {
                    if (capacity_or_output_size == (int)tail_copy_offset)
                        return -1;
                    *(unsigned char *)((long)record_cursor + tail_copy_offset) =
                        *(unsigned char *)(*(long *)((long)context + 0x3b50) + tail_copy_offset);
                    tail_copy_offset = tail_copy_offset + 1;
                } while (tail_copy_offset < *(int *)((long)context + 0x3724));
                capacity_or_output_size = capacity_or_output_size - (int)tail_copy_offset;
                record_cursor = (unsigned char *)((long)record_cursor + tail_copy_offset);
            }
            *(char *)((long)context + 0xbc4) = *(char *)((long)context + 0xbc4) + '\x01';
        }
        *destination = *destination | 0x800;
    } else {
        *(unsigned int *)((long)context + 0x4398) = 0;
        record_cursor = output_cursor;
    }
    output_cursor = record_cursor;
    if ((exclusion_or_record_word >> 0x1e & 1) == 0) {
        if (metadata_output_mode == 1) {
            exclusion_or_record_word = *(int *)((long)context + 0x3af0) + 2;
        copy_level254_fields:
            generated->level254_mode = *(unsigned char *)((long)source + 0x6f);
            *(char *)((long)context + 0x97c) = (char)source[0x1c];
            if (record_cursor != (unsigned char *)0x0)
                goto serialize_level254;
        measure_level254:
            output_cursor = (unsigned char *)0x0;
            capacity = capacity + exclusion_or_record_word + 5;
        } else {
            if (metadata_output_mode != 0) {
                exclusion_or_record_word = 2;
                goto copy_level254_fields;
            }
            exclusion_or_record_word = 2;
            generated->level254_mode = 0;
            generated->level254_version = 2;
            if (record_cursor == (unsigned char *)0x0)
                goto measure_level254;
        serialize_level254:
            if ((((capacity_or_output_size < 4) ||
                  (exclusion_or_record_word = (exclusion_or_record_word & 0xff00ff00) >> 8 | (exclusion_or_record_word & 0xff00ff) << 8,
                   fixed_store_le32(record_cursor, exclusion_or_record_word >> 0x10 | exclusion_or_record_word << 0x10), capacity_or_output_size == 4)) ||
                 (*(unsigned char *)(record_cursor + 4) = 0xfe, capacity_or_output_size == 5)) ||
                (*(unsigned char *)((long)record_cursor + 5) = generated->level254_mode, capacity_or_output_size == 6))
                return -1;
            output_cursor = (unsigned char *)((long)record_cursor + 7);
            capacity_or_output_size = capacity_or_output_size + -7;
            *(unsigned char *)((long)record_cursor + 6) = generated->level254_version;
            if ((metadata_output_mode == 1) && (0 < *(int *)((long)context + 0x3af0))) {
                tail_copy_offset = 0;
                do {
                    if (capacity_or_output_size == (int)tail_copy_offset)
                        return -1;
                    *(unsigned char *)((long)output_cursor + tail_copy_offset) =
                        *(unsigned char *)(*(long *)((long)context + 0x42e8) + tail_copy_offset);
                    tail_copy_offset = tail_copy_offset + 1;
                } while (tail_copy_offset < *(int *)((long)context + 0x3af0));
                capacity_or_output_size = capacity_or_output_size - (int)tail_copy_offset;
                output_cursor = (unsigned char *)((long)output_cursor + tail_copy_offset);
            }
            *(char *)((long)context + 0xbc4) = *(char *)((long)context + 0xbc4) + '\x01';
        }
        *destination = *destination | 0x40000000;
    }
    if (((source == (unsigned int *)0x0) || (*(char *)((long)context + 0x16ec) == '\0')) ||
        ((control->disable_output_metadata != 0 || ((metadata_output_mode != 1 || (revision_or_unknown_size < 4)))))) {
    finish_record_count:
        if (output_cursor != (unsigned char *)0x0) {
        publish_record_count:
            *work->count = *(unsigned char *)((long)context + 0xbc4);
        }
    } else {
        if (output_cursor != (unsigned char *)0x0) {
            revision_or_unknown_size = *(int *)((long)context + 0x16e8);
            if ((mode != 0) || (revision_or_unknown_size <= capacity_or_output_size)) {
                if (revision_or_unknown_size <= capacity_or_output_size) {
                    dovi_MemCpyByte((void *)output_cursor, (const void *)((void *)((long)context + 0x16ed)),
                               (int)revision_or_unknown_size);
                    output_cursor =
                        (unsigned char *)((long)output_cursor + (long)*(int *)((long)context + 0x16e8));
                    capacity_or_output_size = capacity_or_output_size - *(int *)((long)context + 0x16e8);
                    *(char *)((long)context + 0xbc4) =
                        *(char *)((long)context + 0xbc4) + *(char *)((long)context + 0x16ec);
                    goto finish_record_count;
                }
                goto publish_record_count;
            }
            return -1;
        }
        if (mode == 0) {
            revision_or_unknown_size = *(int *)((long)context + 0x16e8);
        } else {
            revision_or_unknown_size = 0;
        }
        capacity = revision_or_unknown_size + capacity;
    }
    capacity_or_output_size = capacity - capacity_or_output_size;

    return capacity_or_output_size;
}

int dovi_dm_GenerateOutputMd(unsigned char *output, unsigned int capacity, unsigned int format,
                             unsigned int flags, int mode, void *context)
{
    _Alignas(16) struct dm_output_workspace work;
    struct metadata_output_state state;
    const struct dm_control *control = &((const struct dm_context_header *)context)->control;
    struct dm_metadata_format *generated = (void *)((long)context + 0x90c);
    unsigned int *destination;
    unsigned int *source;
    unsigned int metadata_output_mode;
    void *selected_input_state;
    void *exclusion_input_state;
    _Bool generate_header;
    _Bool source_missing;
    unsigned int metadata_selection;
    int target_revision;
    int signal_revision;
    int header_selector;
    long kernel_map;
    unsigned long remaining_capacity;
    unsigned char *output_cursor;
    unsigned int *input_signal_words;
    metadata_output_mode = *(unsigned int *)((long)context + 0x43bc);
    metadata_selection = control->primary_signal;
    target_revision = *(int *)((long)context + 0x89c);
    selected_input_state = context;
    if (*(unsigned int *)((long)context + 0x43b8) == metadata_output_mode) {
        if (5 < metadata_selection)
            dm_limit_trap();
        selected_input_state = (void *)((long)context + (unsigned long)metadata_selection * 0x1f78);
    }
    input_signal_words = (unsigned int *)((long)selected_input_state + 0xe240);
    signal_revision = *(int *)((long)selected_input_state + 0xe2a8);
    work.trims.pairs[6] = 0;
    work.trims.pairs[7] = 0;
    work.trims.pairs[4] = 0;
    work.trims.pairs[5] = 0;
    work.trims.pairs[2] = 0;
    work.trims.pairs[3] = 0;
    kernel_map = *(long *)(*(long *)((long)context + (long)(int)(metadata_selection - 1) * 8 + 0x1f918) + 0x15f8);
    work.trims.pairs[0] = 0;
    work.trims.pairs[1] = 0;
    work.trim_count = 0;
    exclusion_input_state = context;
    if (*(unsigned int *)((long)context + 0x43b8) == metadata_output_mode) {
        if (5 < metadata_selection)
            dm_limit_trap();
        exclusion_input_state = (void *)((long)context + (unsigned long)metadata_selection * 0x1f78);
    }
    if (4 < metadata_selection)
        dm_limit_trap();
    source = (unsigned int *)0x0;
    if (*(int *)((long)context + (unsigned long)metadata_selection * 0x2c8 + 0x908) != 0) {
        source = (unsigned int *)((long)context + (unsigned long)metadata_selection * 0x2c8 + 0x90c);
    }
    metadata_selection = output_metadata_exclusions(source, flags, mode, target_revision, metadata_output_mode, exclusion_input_state, context);
    destination = (unsigned int *)((long)context + 0x90c);
    header_selector = *(int *)((long)selected_input_state + 0xe318);
    *destination = 0;
    generate_header = header_selector != 2;
    source_missing = source == (unsigned int *)0x0;
    if (output != (unsigned char *)0x0) {
        struct metadata_output_cursor cursor;

        if (!write_output_metadata_header(output, capacity, source, metadata_output_mode, selected_input_state, kernel_map, header_selector,
                                          context, &cursor))
            return -1;
        work.count = cursor.count;
        output_cursor = cursor.records;
        remaining_capacity = cursor.remaining;
    } else {
        remaining_capacity = 0;
        work.count = (unsigned char *)0x0;
        capacity = 0x47;
        output_cursor = (unsigned char *)0x0;
    }
    state = (struct metadata_output_state){
        .context = context,
        .signal_context = selected_input_state,
        .signal_words = input_signal_words,
        .source = source,
        .control = control,
        .generated = generated,
        .cursor = {work.count, output_cursor, remaining_capacity},
        .measured = capacity,
        .exclusions = metadata_selection,
        .output_mode = metadata_output_mode,
        .format = format,
        .mode = mode,
        .target_version = target_revision,
        .signal_version = signal_revision,
        .synthetic_header = generate_header,
        .missing_source = source_missing,
    };
    if (!write_legacy_metadata_extensions(&state, &work) || !write_level8_metadata(&state, &work) ||
        !write_primaries_metadata(&state, &work) || !write_level10_metadata(&state, &work))
        return -1;
    return finish_output_metadata(&state, &work);
}
