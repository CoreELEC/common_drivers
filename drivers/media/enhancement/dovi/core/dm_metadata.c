/* Input display metadata decoding and configuration. */
#include "dm_bindings.h"
#include "dm_layout.h"
#include "fixed_access.h"

struct metadata_extension_cursor {
    const unsigned char *source;
    const unsigned char *next;
    unsigned int available;
    unsigned int payload_size;
    unsigned int skip;
    unsigned int tail;
};

static int parse_level8_metadata(struct metadata_extension_cursor *cursor, unsigned int *metadata,
                                 int input_index, const void *configuration, long context)
{
    int optional_byte_groups;
    long tail_destination;
    long tail_advance;
    short *optional_word_target;
    struct dm_metadata_format *decoded = (void *)metadata;
    const unsigned char *source = cursor->source;
    unsigned int trim_index;
    unsigned int payload_or_field_count = cursor->payload_size;
    unsigned int payload_skip;
    unsigned int available_input_bytes = cursor->available;
    unsigned int available_tail = cursor->tail;
    unsigned long tail_bytes_work;
    unsigned long optional_byte_index;
    unsigned long optional_or_tail_offset;
    const unsigned char *field_cursor;
    const unsigned char *next_payload_byte = cursor->next;
    trim_index = metadata[0x46];
    if (4 < (int)trim_index) {
        payload_skip = fixed_load_be32(source);
        goto finish_level;
    }
    if ((int)((const struct dm_control *)configuration)->primary_signal == input_index) {
        if (4 < trim_index)
            dm_limit_trap();
        *(unsigned int *)(context + (unsigned long)trim_index * 4 + 0x42f8) = payload_or_field_count;
        if (4 < metadata[0x46])
            dm_limit_trap();
        *(unsigned int *)(context + (unsigned long)metadata[0x46] * 4 + 0x430c) = 0;
        trim_index = metadata[0x46];
    }
    if ((int)payload_or_field_count < 1) {
        return -1;
    }
    if (available_tail == 0) {
        return -1;
    }
    payload_skip = payload_or_field_count - 0xd;
    *(unsigned short *)((long)metadata + (long)(int)trim_index * 0x2a + 0x11c) =
        (unsigned short)(unsigned char)fixed_load_le16(next_payload_byte);
    if (0xfffffff3 < payload_skip) {
        return -1;
    }
    if (available_input_bytes < 8) {
        return -1;
    }
    *(unsigned short *)((long)metadata + (long)(int)trim_index * 0x2a + 0x11e) =
        fixed_load_be16((const void *)(source + 6));
    if (available_input_bytes < 10) {
        return -1;
    }
    *(unsigned short *)((long)metadata + (long)(int)trim_index * 0x2a + 0x120) =
        fixed_load_be16((const void *)(source + 8));
    if (available_input_bytes < 0xc) {
        return -1;
    }
    *(unsigned short *)((long)metadata + (long)(int)trim_index * 0x2a + 0x122) =
        fixed_load_be16((const void *)(source + 10));
    if (available_input_bytes < 0xe) {
        return -1;
    }
    *(unsigned short *)((long)metadata + (long)(int)trim_index * 0x2a + 0x124) =
        fixed_load_be16((const void *)(source + 0xc));
    if (available_input_bytes < 0x10) {
        return -1;
    }
    available_tail = available_input_bytes - 0x12;
    *(unsigned short *)((long)metadata + (long)(int)trim_index * 0x2a + 0x126) =
        fixed_load_be16((const void *)(source + 0xe));
    if (available_input_bytes < 0x12) {
        return -1;
    }
    field_cursor = (const unsigned char *)(source + 0x12);
    *(unsigned short *)((long)metadata + (long)(int)trim_index * 0x2a + 0x128) =
        fixed_load_be16((const void *)(source + 0x10));
    dovi_ApplyLocalMs((const void *)configuration, (void *)(metadata + 0x44));
    payload_or_field_count = payload_skip >> 1 & 0x7f;
    optional_word_target = (short *)((long)metadata + (long)(int)trim_index * 0x2a + 0x12a);
    *optional_word_target = (short)*(const unsigned int *)((long)configuration + 0x7a4) -
               (short)*(const unsigned int *)((long)configuration + 0x7f8);
    *(short *)((long)metadata + (long)(int)trim_index * 0x2a + 300) =
        (short)*(const unsigned int *)((long)configuration + 0x7a8) -
        (short)*(const unsigned int *)((long)configuration + 0x7fc);
    *(short *)((long)metadata + (long)(int)trim_index * 0x2a + 0x12e) =
        (short)*(const unsigned int *)((long)configuration + 0x7ac) -
        (short)*(const unsigned int *)((long)configuration + 0x800);
    *(short *)((long)metadata + (long)(int)trim_index * 0x2a + 0x130) =
        (short)*(const unsigned int *)((long)configuration + 0x7b0) -
        (short)*(const unsigned int *)((long)configuration + 0x804);
    *(short *)((long)metadata + (long)(int)trim_index * 0x2a + 0x132) =
        (short)*(const unsigned int *)((long)configuration + 0x7b4) -
        (short)*(const unsigned int *)((long)configuration + 0x808);
    *(short *)((long)metadata + (long)(int)trim_index * 0x2a + 0x134) =
        (short)*(const unsigned int *)((long)configuration + 0x7b8) -
        (short)*(const unsigned int *)((long)configuration + 0x80c);
    *(short *)((long)metadata + (long)(int)trim_index * 0x2a + 0x136) =
        (short)*(const unsigned int *)((long)configuration + 0x7bc) -
        (short)*(const unsigned int *)((long)configuration + 0x810);
    *(short *)((long)metadata + (long)(int)trim_index * 0x2a + 0x138) =
        (short)*(const unsigned int *)((long)configuration + 0x7c0) -
        (short)*(const unsigned int *)((long)configuration + 0x814);
    *(short *)((long)metadata + (long)(int)trim_index * 0x2a + 0x13a) =
        (short)*(const unsigned int *)((long)configuration + 0x7c4) -
        (short)*(const unsigned int *)((long)configuration + 0x818);
    *(short *)((long)metadata + (long)(int)trim_index * 0x2a + 0x13c) =
        (short)*(const unsigned int *)((long)configuration + 0x7c8) -
        (short)*(const unsigned int *)((long)configuration + 0x81c);
    *(short *)((long)metadata + (long)(int)trim_index * 0x2a + 0x13e) =
        (short)*(const unsigned int *)((long)configuration + 0x7cc) -
        (short)*(const unsigned int *)((long)configuration + 0x820);
    *(short *)((long)metadata + (long)(int)trim_index * 0x2a + 0x140) =
        (short)*(const unsigned int *)((long)configuration + 2000) -
        (short)*(const unsigned int *)((long)configuration + 0x824);
    *(short *)((long)metadata + (long)(int)trim_index * 0x2a + 0x142) =
        (short)*(const unsigned int *)((long)configuration + 0x7d4) -
        (short)*(const unsigned int *)((long)configuration + 0x828);
    if (5 < payload_skip) {
        payload_or_field_count = 2;
    }
    *(short *)((long)metadata + (long)(int)trim_index * 0x2a + 0x144) =
        (short)*(const unsigned int *)((long)configuration + 0x7d8) -
        (short)*(const unsigned int *)((long)configuration + 0x82c);
    next_payload_byte = field_cursor;
    if (payload_or_field_count != 0) {
        optional_or_tail_offset = 0;
        do {
            if ((available_tail & 0xfffffffe) == optional_or_tail_offset) {
                return -1;
            }
            *(unsigned short *)((long)optional_word_target + optional_or_tail_offset) =
                fixed_load_be16((const void *)((long)field_cursor + optional_or_tail_offset));
            optional_or_tail_offset = optional_or_tail_offset + 2;
        } while ((unsigned long)payload_or_field_count << 1 != optional_or_tail_offset);
        payload_skip = payload_skip + payload_or_field_count * -2;
        tail_bytes_work = available_tail - optional_or_tail_offset;
        available_tail = (unsigned int)tail_bytes_work;
        next_payload_byte = (const unsigned char *)((long)field_cursor + optional_or_tail_offset);
        if (payload_or_field_count == 2) {
            optional_byte_groups = (int)payload_skip / 6;
            if (0x11 < (int)payload_skip) {
                optional_byte_groups = 2;
            }
            if (optional_byte_groups != 0) {
                payload_or_field_count = optional_byte_groups * 6;
                if (optional_byte_groups < 1) {
                    payload_skip = payload_skip + optional_byte_groups * -6;
                } else {
                    optional_byte_index = 0;
                    if ((int)payload_or_field_count < 2) {
                        payload_or_field_count = 1;
                    }
                    do {
                        if ((0x12 - available_input_bytes) + (int)optional_or_tail_offset + (int)optional_byte_index == 0) {
                            return -1;
                        }
                        available_tail = (int)tail_bytes_work - 1;
                        tail_bytes_work = (unsigned long)available_tail;
                        *(unsigned short *)((long)metadata + optional_byte_index * 2 + (long)(int)trim_index * 0x2a +
                                            0x12e) =
                            (unsigned short)*(const unsigned char *)((long)field_cursor + optional_byte_index +
                                                                     optional_or_tail_offset);
                        optional_byte_index = optional_byte_index + 1;
                    } while (payload_or_field_count != optional_byte_index);
                    payload_skip = payload_skip + optional_byte_groups * -6;
                    next_payload_byte = (const unsigned char *)((long)field_cursor + optional_byte_index + optional_or_tail_offset);
                    if ((optional_byte_groups == 2) &&
                        ((int)((const struct dm_control *)configuration)->primary_signal ==
                         input_index)) {
                        if (0 < (int)payload_skip) {
                            optional_or_tail_offset = 0;
                            field_cursor = next_payload_byte;
                            do {
                                if ((int)tail_bytes_work == 0) {
                                    return -1;
                                }
                                available_tail = (int)tail_bytes_work - 1;
                                tail_bytes_work = (unsigned long)available_tail;
                                next_payload_byte = (const unsigned char *)((long)field_cursor + 1);
                                *(unsigned char *)(*(long *)(context + 0x36f0) + optional_or_tail_offset) =
                                    (unsigned char)fixed_load_le16(field_cursor);
                                optional_or_tail_offset = optional_or_tail_offset + 1;
                                field_cursor = next_payload_byte;
                            } while (payload_skip != optional_or_tail_offset);
                        }
                        if (4 < metadata[0x46])
                            dm_limit_trap();
                        *(unsigned int *)(context + (unsigned long)metadata[0x46] * 4 + 0x430c) =
                            payload_skip;
                        if (4 < metadata[0x46])
                            dm_limit_trap();
                        tail_destination = *(long *)(context + 0x36f0);
                        tail_advance = (long)(int)payload_skip;
                        payload_skip = 0;
                        *(long *)(context + (unsigned long)metadata[0x46] * 8 + 0x4320) = tail_destination;
                        *(long *)(context + 0x36f0) = tail_destination + tail_advance;
                    }
                }
            }
        }
    }
    decoded->flags = decoded->flags | 0x100;
    metadata[0x46] = metadata[0x46] + 1;
    goto finish_level;

finish_level:
    cursor->next = next_payload_byte;
    cursor->skip = payload_skip;
    cursor->tail = available_tail;
    return 0;
}

static int parse_level10_metadata(struct metadata_extension_cursor *cursor, unsigned int *metadata,
                                  int input_index, const void *configuration, long context)
{
    long tail_destination;
    long tail_advance;
    struct dm_metadata_format *decoded = (void *)metadata;
    const unsigned char *source = cursor->source;
    unsigned char primaries_index;
    unsigned int payload_remainder;
    unsigned int declared_payload_size = cursor->payload_size;
    unsigned int record_index_or_skip;
    unsigned int available_input_bytes = cursor->available;
    unsigned int available_tail = cursor->tail;
    unsigned long tail_copy_offset;
    unsigned long unknown_tail_size;
    const unsigned char *next_payload_byte = cursor->next;
    unsigned short custom_coordinate_1;
    unsigned short custom_coordinate_3;
    unsigned short custom_coordinate_5;
    unsigned short custom_coordinate_7;
    record_index_or_skip = metadata[0x88];
    if (4 < (int)record_index_or_skip) {
        record_index_or_skip = fixed_load_be32(source);
        goto finish_level;
    }
    if ((int)((const struct dm_control *)configuration)->primary_signal == input_index) {
        if (4 < record_index_or_skip)
            dm_limit_trap();
        *(unsigned int *)(context + (unsigned long)record_index_or_skip * 4 + 0x4348) = declared_payload_size;
        if (4 < metadata[0x88])
            dm_limit_trap();
        *(unsigned int *)(context + (unsigned long)metadata[0x88] * 4 + 0x435c) = 0;
        record_index_or_skip = metadata[0x88];
    }
    if ((int)declared_payload_size < 1) {
        return -1;
    }
    if (available_tail == 0) {
        return -1;
    }
    payload_remainder = declared_payload_size - 6;
    unknown_tail_size = (unsigned long)payload_remainder;
    *(unsigned short *)(metadata + (long)(int)record_index_or_skip * 6 + 0x89) =
        (unsigned short)(unsigned char)fixed_load_le16(next_payload_byte);
    if (0xfffffffa < payload_remainder) {
        return -1;
    }
    if (available_input_bytes < 8) {
        return -1;
    }
    *(unsigned short *)((long)metadata + (long)(int)record_index_or_skip * 0x18 + 0x226) =
        fixed_load_be16((const void *)(source + 6));
    if (available_input_bytes < 10) {
        return -1;
    }
    *(unsigned short *)(metadata + (long)(int)record_index_or_skip * 6 + 0x8a) =
        fixed_load_be16((const void *)(source + 8));
    if (available_input_bytes == 10) {
        return -1;
    }
    primaries_index = source[10];
    available_tail = available_input_bytes - 0xb;
    *(unsigned short *)((long)metadata + (long)(int)record_index_or_skip * 0x18 + 0x22a) = (unsigned short)primaries_index;
    if (primaries_index < 0x13) {
        next_payload_byte = (const unsigned char *)(source + 0xb);
    } else {
        if (payload_remainder < 0x10) {
            return -1;
        }
        if (available_tail < 2) {
            return -1;
        }
        available_tail = available_tail >> 1;
        *(unsigned short *)(metadata + (long)(int)record_index_or_skip * 6 + 0x8b) =
            fixed_load_be16((const void *)(source + 0xb));
        if (available_tail == 1) {
            return -1;
        }
        custom_coordinate_1 = fixed_load_be16((const void *)(source + 0xd));
        *(unsigned short *)((long)metadata + (long)(int)record_index_or_skip * 0x18 + 0x22e) = custom_coordinate_1;
        if (available_tail == 2) {
            return -1;
        }
        *(unsigned short *)(metadata + (long)(int)record_index_or_skip * 6 + 0x8c) =
            fixed_load_be16((const void *)(source + 0xf));
        if (available_tail == 3) {
            return -1;
        }
        custom_coordinate_3 = fixed_load_be16((const void *)(source + 0x11));
        *(unsigned short *)((long)metadata + (long)(int)record_index_or_skip * 0x18 + 0x232) = custom_coordinate_3;
        if (available_tail == 4) {
            return -1;
        }
        *(unsigned short *)(metadata + (long)(int)record_index_or_skip * 6 + 0x8d) =
            fixed_load_be16((const void *)(source + 0x13));
        if (available_tail == 5) {
            return -1;
        }
        custom_coordinate_5 = fixed_load_be16((const void *)(source + 0x15));
        *(unsigned short *)((long)metadata + (long)(int)record_index_or_skip * 0x18 + 0x236) = custom_coordinate_5;
        if (available_tail == 6) {
            return -1;
        }
        *(unsigned short *)(metadata + (long)(int)record_index_or_skip * 6 + 0x8e) =
            fixed_load_be16((const void *)(source + 0x17));
        if (available_tail == 7) {
            return -1;
        }
        next_payload_byte = (const unsigned char *)(source + 0x1b);
        available_tail = available_input_bytes - 0x1b;
        custom_coordinate_7 = fixed_load_be16((const void *)(source + 0x19));
        *(unsigned short *)((long)metadata + (long)(int)record_index_or_skip * 0x18 + 0x23a) = custom_coordinate_7;
        if (((custom_coordinate_1 == 0) && (custom_coordinate_3 == 0)) && (custom_coordinate_5 == 0)) {
            record_index_or_skip = declared_payload_size - 0x16;
            unknown_tail_size = (unsigned long)record_index_or_skip;
            if (custom_coordinate_7 == 0)
                goto finish_level;
        } else {
            unknown_tail_size = (unsigned long)(declared_payload_size - 0x16);
        }
    }
    record_index_or_skip = (unsigned int)unknown_tail_size;
    if ((int)((const struct dm_control *)configuration)->primary_signal == input_index) {
        if (record_index_or_skip != 0) {
            tail_copy_offset = 0;
            do {
                if (available_tail == (unsigned int)tail_copy_offset) {
                    return -1;
                }
                *(unsigned char *)(*(long *)(context + 0x36f0) + tail_copy_offset) =
                    *(const unsigned char *)((long)next_payload_byte + tail_copy_offset);
                tail_copy_offset = tail_copy_offset + 1;
            } while (unknown_tail_size != tail_copy_offset);
            available_tail = available_tail - (int)tail_copy_offset;
            next_payload_byte = (const unsigned char *)((long)next_payload_byte + tail_copy_offset);
        }
        if (4 < metadata[0x88])
            dm_limit_trap();
        *(unsigned int *)(context + (unsigned long)metadata[0x88] * 4 + 0x435c) = record_index_or_skip;
        if (4 < metadata[0x88])
            dm_limit_trap();
        tail_destination = *(long *)(context + 0x36f0);
        tail_advance = (long)(int)record_index_or_skip;
        record_index_or_skip = 0;
        *(long *)(context + (unsigned long)metadata[0x88] * 8 + 0x4370) = tail_destination;
        *(long *)(context + 0x36f0) = tail_destination + tail_advance;
    }
    decoded->flags = decoded->flags | 0x400;
    metadata[0x88] = metadata[0x88] + 1;
    goto finish_level;

finish_level:
    cursor->next = next_payload_byte;
    cursor->skip = record_index_or_skip;
    cursor->tail = available_tail;
    return 0;
}

/* Extension records retain their own length checks and partial writes. */
static int parse_metadata_extensions(const unsigned char *input, unsigned int size,
                                     unsigned int *metadata, int input_index,
                                     const void *configuration, long context)
{
    struct dm_metadata_format *decoded = (void *)metadata;
    long tail_advance;
    int aggregate_record_size;
    char *aggregate_record_count;
    char extension_count;
    unsigned char extension_level;
    unsigned short custom_coordinate_1;
    unsigned short custom_coordinate_3;
    unsigned short custom_coordinate_5;
    unsigned short custom_coordinate_7;
    _Bool retain_unknown_tail;
    const unsigned char *source;
    unsigned int optional_level11_count;
    unsigned char metadata_byte;
    unsigned long tail_copy_offset;
    long tail_destination;
    unsigned int declared_payload_size;
    unsigned long payload_size_or_field_index;
    unsigned int payload_skip;
    unsigned int available_or_flag_word;
    const unsigned char *field_cursor;
    const unsigned char *next_payload_byte;
    unsigned int extension_index;
    unsigned int available_tail;

    aggregate_record_count = (char *)(context + 0x16ec);
    extension_count = input[0x46];
    available_or_flag_word = size - 0x47;
    *(char *)(metadata + 0xae) = extension_count;
    if (extension_count != '\0') {
        extension_index = 0;
        source = input + 0x47;
        retain_unknown_tail = 1;
        do {
            if ((int)available_or_flag_word < 4) {
                return -1;
            }
            if (available_or_flag_word == 4) {
                return -1;
            }
            available_tail = available_or_flag_word - 5;
            payload_size_or_field_index = (long)(int)((unsigned int)*source << 0x18) | (unsigned long)source[1] << 0x10 |
                     (unsigned long)source[2] << 8 | (unsigned long)source[3];
            declared_payload_size = (unsigned int)payload_size_or_field_index;
            if ((int)available_tail < (int)declared_payload_size) {
                return -1;
            }
            extension_level = source[4];
            next_payload_byte = (const unsigned char *)(source + 5);
            switch (extension_level) {
            case 1:
                payload_skip = declared_payload_size - 6;
                if ((int)declared_payload_size < 6) {
                    return -1;
                }
                if (available_tail < 2) {
                    return -1;
                }
                *(unsigned short *)(metadata + 0x1e) =
                    fixed_load_be16((const void *)(source + 5));
                if (0xfffffffd < available_or_flag_word - 9) {
                    return -1;
                }
                available_tail = available_or_flag_word - 0xb;
                *(unsigned short *)((long)metadata + 0x7a) =
                    fixed_load_be16((const void *)(source + 7));
                if (0xfffffffd < available_tail) {
                    return -1;
                }
                next_payload_byte = (const unsigned char *)(source + 0xb);
                *(unsigned short *)(metadata + 0x1f) =
                    fixed_load_be16((const void *)(source + 9));
                decoded->flags = decoded->flags | 2;
                break;
            case 2:
                if ((int)metadata[0x23] < 8) {
                    if ((int)declared_payload_size < (int)(metadata[0x22] * 2)) {
                        return -1;
                    }
                    if (0 < (int)metadata[0x22]) {
                        payload_size_or_field_index = 0;
                        available_or_flag_word = available_tail >> 1;
                        field_cursor = next_payload_byte;
                        do {
                            if (available_or_flag_word == payload_size_or_field_index) {
                                return -1;
                            }
                            next_payload_byte = field_cursor + 2;
                            available_tail = available_tail - 2;
                            *(unsigned short *)((long)metadata + payload_size_or_field_index * 2 +
                                                (long)(int)metadata[0x23] * 0xe + 0x90) =
                                fixed_load_be16(field_cursor);
                            payload_size_or_field_index = payload_size_or_field_index + 1;
                            field_cursor = next_payload_byte;
                        } while ((long)payload_size_or_field_index < (long)(int)metadata[0x22]);
                    }
                    dovi_ApplyLocalMs((const void *)configuration, (void *)(metadata + 0x21));
                    payload_skip = declared_payload_size + metadata[0x22] * -2;
                    decoded->flags = decoded->flags | 4;
                    metadata[0x23] = metadata[0x23] + 1;
                } else {
                skip_payload:
                    payload_skip = fixed_load_be32(source);
                }
                break;
            case 3:
                payload_skip = declared_payload_size - 6;
                if ((int)declared_payload_size < 6) {
                    return -1;
                }
                if (available_tail < 2) {
                    return -1;
                }
                *(unsigned short *)((long)metadata + 0x7e) =
                    fixed_load_be16((const void *)(source + 5));
                if (0xfffffffd < available_or_flag_word - 9) {
                    return -1;
                }
                available_tail = available_or_flag_word - 0xb;
                *(unsigned short *)(metadata + 0x20) =
                    fixed_load_be16((const void *)(source + 7));
                if (0xfffffffd < available_tail) {
                    return -1;
                }
                next_payload_byte = (const unsigned char *)(source + 0xb);
                *(unsigned short *)((long)metadata + 0x82) =
                    fixed_load_be16((const void *)(source + 9));
                decoded->flags = decoded->flags | 8;
                break;
            case 4:
                payload_skip = declared_payload_size - 4;
                if ((int)declared_payload_size < 4) {
                    return -1;
                }
                if (available_tail < 2) {
                    return -1;
                }
                available_tail = available_or_flag_word - 9;
                *(unsigned short *)(metadata + 0xad) =
                    fixed_load_be16((const void *)(source + 5));
                if (0xfffffffd < available_tail) {
                    return -1;
                }
                next_payload_byte = (const unsigned char *)(source + 9);
                *(unsigned short *)((long)metadata + 0x2b6) =
                    fixed_load_be16((const void *)(source + 7));
                decoded->flags = decoded->flags | 0x10;
                break;
            case 5:
                payload_skip = declared_payload_size - 8;
                if ((int)declared_payload_size < 8) {
                    return -1;
                }
                if (available_tail < 2) {
                    return -1;
                }
                decoded->active_left = fixed_load_be16((const void *)(source + 5));
                if (0xfffffffd < available_or_flag_word - 9) {
                    return -1;
                }
                decoded->active_right = fixed_load_be16((const void *)(source + 7));
                if (0xfffffffd < available_or_flag_word - 0xb) {
                    return -1;
                }
                available_tail = available_or_flag_word - 0xd;
                decoded->active_top = fixed_load_be16((const void *)(source + 9));
                if (0xfffffffd < available_tail) {
                    return -1;
                }
                next_payload_byte = (const unsigned char *)(source + 0xd);
                decoded->active_bottom = fixed_load_be16((const void *)(source + 0xb));
                decoded->flags = decoded->flags | 0x20;
                break;
            case 6:
            case 7:
            retain_unknown_record:
                if (((int)((const struct dm_control *)configuration)->primary_signal !=
                     input_index) ||
                    (extension_level == 6))
                    goto skip_payload;
                available_or_flag_word = *(unsigned int *)(context + 0x16e8);
                aggregate_record_size = declared_payload_size + 5;
                if (0x1000 < (int)(available_or_flag_word + aggregate_record_size)) {
                    return -1;
                }
                if (0x1000 < available_or_flag_word) {
                    dm_limit_trap();
                }
                dovi_MemCpyByte((void *)(context + 0x16ed + (unsigned long)available_or_flag_word), (const void *)source,
                           (int)aggregate_record_size);
                payload_skip = 0;
                next_payload_byte = (const unsigned char *)((long)next_payload_byte + payload_size_or_field_index);
                *(int *)(context + 0x16e8) = *(int *)(context + 0x16e8) + aggregate_record_size;
                *aggregate_record_count = *aggregate_record_count + '\x01';
                available_tail = available_tail - declared_payload_size;
                break;
            case 8: {
                struct metadata_extension_cursor cursor = {source, next_payload_byte, available_or_flag_word,
                                                           declared_payload_size, 0,       available_tail};
                if (parse_level8_metadata(&cursor, metadata, input_index, configuration, context) <
                    0)
                    return -1;
                next_payload_byte = cursor.next;
                payload_skip = cursor.skip;
                available_tail = cursor.tail;
                break;
            }
            case 9:
                payload_size_or_field_index = (unsigned long)(declared_payload_size - 1);
                if ((int)declared_payload_size < 1) {
                    return -1;
                }
                if (available_tail == 0) {
                    return -1;
                }
                metadata_byte = source[5];
                decoded->primaries_index = metadata_byte;
                if (metadata_byte < 0x13) {
                    next_payload_byte = (const unsigned char *)(source + 6);
                    available_tail = available_or_flag_word - 6;
                } else {
                    if (declared_payload_size - 1 < 0x10) {
                        return -1;
                    }
                    if (available_or_flag_word < 8) {
                        return -1;
                    }
                    decoded->primaries[0] = fixed_load_be16((const void *)(source + 6));
                    if (available_or_flag_word < 10) {
                        return -1;
                    }
                    custom_coordinate_1 = fixed_load_be16((const void *)(source + 8));
                    decoded->primaries[1] = custom_coordinate_1;
                    if (available_or_flag_word < 0xc) {
                        return -1;
                    }
                    decoded->primaries[2] = fixed_load_be16((const void *)(source + 10));
                    if (available_or_flag_word < 0xe) {
                        return -1;
                    }
                    custom_coordinate_3 = fixed_load_be16((const void *)(source + 0xc));
                    decoded->primaries[3] = custom_coordinate_3;
                    if (available_or_flag_word < 0x10) {
                        return -1;
                    }
                    decoded->primaries[4] = fixed_load_be16((const void *)(source + 0xe));
                    if (available_or_flag_word < 0x12) {
                        return -1;
                    }
                    custom_coordinate_5 = fixed_load_be16((const void *)(source + 0x10));
                    decoded->primaries[5] = custom_coordinate_5;
                    if (available_or_flag_word < 0x14) {
                        return -1;
                    }
                    available_tail = available_or_flag_word - 0x16;
                    decoded->primaries[6] = fixed_load_be16((const void *)(source + 0x12));
                    if (available_or_flag_word < 0x16) {
                        return -1;
                    }
                    next_payload_byte = (const unsigned char *)(source + 0x16);
                    custom_coordinate_7 = fixed_load_be16((const void *)(source + 0x14));
                    decoded->primaries[7] = custom_coordinate_7;
                    if (((custom_coordinate_1 == 0) && (custom_coordinate_3 == 0)) && (custom_coordinate_5 == 0)) {
                        payload_size_or_field_index = (unsigned long)(declared_payload_size - 0x11);
                        if (custom_coordinate_7 == 0)
                            goto retain_extension_tail;
                    } else {
                        payload_size_or_field_index = (unsigned long)(declared_payload_size - 0x11);
                    }
                }
                available_or_flag_word = decoded->flags | 0x200;
            publish_extension_flags:
                decoded->flags = available_or_flag_word;
            retain_extension_tail:
                payload_skip = (unsigned int)payload_size_or_field_index;
                if (((int)((const struct dm_control *)configuration)->primary_signal ==
                     input_index) &&
                    (retain_unknown_tail)) {
                    if (0 < (int)payload_skip) {
                        tail_copy_offset = 0;
                        do {
                            if (available_tail == (unsigned int)tail_copy_offset) {
                                return -1;
                            }
                            *(unsigned char *)(*(long *)(context + 0x36f0) + tail_copy_offset) =
                                *(const unsigned char *)((long)next_payload_byte + tail_copy_offset);
                            tail_copy_offset = tail_copy_offset + 1;
                        } while (payload_size_or_field_index != tail_copy_offset);
                        available_tail = available_tail - (int)tail_copy_offset;
                        next_payload_byte = (const unsigned char *)((long)next_payload_byte + tail_copy_offset);
                    }
                    tail_destination = *(long *)(context + 0x36f0);
                    *(unsigned int *)(context + (unsigned long)extension_level * 4 + 0x36f8) = payload_skip;
                    tail_advance = (long)(int)payload_skip;
                    retain_unknown_tail = 1;
                    payload_skip = 0;
                    *(long *)(context + (unsigned long)extension_level * 8 + 0x3af8) = tail_destination;
                    *(long *)(context + 0x36f0) = tail_destination + tail_advance;
                }
                break;
            case 10: {
                struct metadata_extension_cursor cursor = {source, next_payload_byte, available_or_flag_word,
                                                           declared_payload_size, 0,       available_tail};
                if (parse_level10_metadata(&cursor, metadata, input_index, configuration, context) <
                    0)
                    return -1;
                next_payload_byte = cursor.next;
                payload_skip = cursor.skip;
                available_tail = cursor.tail;
                break;
            }
            case 0xb:
                payload_skip = declared_payload_size - 2;
                payload_size_or_field_index = (unsigned long)payload_skip;
                if ((int)declared_payload_size < 2) {
                    return -1;
                }
                if (available_tail == 0) {
                    return -1;
                }
                metadata_byte = source[5];
                decoded->level11_content_type = metadata_byte;
                if ((int)((const struct dm_control *)configuration)->primary_signal ==
                    input_index) {
                    *(unsigned char *)(context + 0x439c) = metadata_byte;
                    metadata_byte = decoded->level11_content_type;
                }
                decoded->level11_white_point = 0;
                decoded->level11_content_type = metadata_byte & 0xf;
                if (available_or_flag_word == 6) {
                    return -1;
                }
                metadata_byte = source[6];
                decoded->level11_reference_mode = metadata_byte;
                if ((int)((const struct dm_control *)configuration)->primary_signal ==
                    input_index) {
                    *(unsigned char *)(context + 0x439d) = metadata_byte;
                    metadata_byte = decoded->level11_reference_mode;
                }
                next_payload_byte = (const unsigned char *)(source + 7);
                available_tail = available_or_flag_word - 7;
                decoded->level11_reference_mode = metadata_byte & 0xf;
                decoded->flags = decoded->flags | 0x800;
                if ((int)((const struct dm_control *)configuration)->primary_signal != input_index)
                    goto retain_extension_tail;
                optional_level11_count = payload_skip;
                if (1 < (int)payload_skip) {
                    optional_level11_count = 2;
                }
                if (declared_payload_size < 3) {
                    payload_skip = payload_skip - optional_level11_count;
                    *(unsigned int *)(context + 0x4398) = optional_level11_count + 2;
                } else {
                    if (available_tail == 0) {
                        return -1;
                    }
                    available_tail = available_or_flag_word - 8;
                    *(unsigned char *)(context + 0x439e) = source[7];
                    if ((int)declared_payload_size < 4) {
                        next_payload_byte = (const unsigned char *)(source + 8);
                    } else {
                        if (available_tail == 0) {
                            return -1;
                        }
                        next_payload_byte = (const unsigned char *)(source + 9);
                        available_tail = available_or_flag_word - 9;
                        *(unsigned char *)(context + 0x439f) = source[8];
                    }
                    payload_skip = payload_skip - optional_level11_count;
                    payload_size_or_field_index = (unsigned long)payload_skip;
                    *(unsigned int *)(context + 0x4398) = optional_level11_count + 2;
                    if (3 < (int)declared_payload_size)
                        goto retain_extension_tail;
                }
                retain_unknown_tail = 0;
                *(unsigned int *)(context + 0x3724) = 0;
                break;
            default:
                if (extension_level == 0xfe) {
                    payload_size_or_field_index = (unsigned long)(declared_payload_size - 2);
                    if ((int)declared_payload_size < 2) {
                        return -1;
                    }
                    if (available_tail == 0) {
                        return -1;
                    }
                    decoded->level254_mode = (unsigned char)fixed_load_le16(next_payload_byte);
                    if (available_or_flag_word == 6) {
                        return -1;
                    }
                    next_payload_byte = (const unsigned char *)(source + 7);
                    available_tail = available_or_flag_word - 7;
                    available_or_flag_word = decoded->flags | 0x40000000;
                    decoded->level254_version = source[6];
                    goto publish_extension_flags;
                }
                if (extension_level != 0xff)
                    goto retain_unknown_record;
                payload_skip = declared_payload_size - 6;
                if ((int)declared_payload_size < 6) {
                    return -1;
                }
                if (available_tail == 0) {
                    return -1;
                }
                decoded->run_mode = (unsigned char)fixed_load_le16(next_payload_byte);
                if (available_or_flag_word == 6) {
                    return -1;
                }
                decoded->run_version = source[6];
                if (available_or_flag_word == 7) {
                    return -1;
                }
                decoded->debug[0] = source[7];
                if (available_or_flag_word == 8) {
                    return -1;
                }
                decoded->debug[1] = source[8];
                if (available_or_flag_word == 9) {
                    return -1;
                }
                decoded->debug[2] = source[9];
                if (available_or_flag_word == 10) {
                    return -1;
                }
                next_payload_byte = (const unsigned char *)(source + 0xb);
                decoded->debug[3] = source[10];
                decoded->flags = decoded->flags | 0x80000000;
                available_tail = available_or_flag_word - 0xb;
            }
            if ((int)payload_skip < 0) {
                return -1;
            }
            available_or_flag_word = available_tail - payload_skip;
            if ((int)available_tail < (int)payload_skip) {
                return -1;
            }
            source = (const unsigned char *)((long)next_payload_byte + (unsigned long)payload_skip);
            extension_index = extension_index + 1;
        } while (extension_index < (unsigned char)metadata[0xae]);
    }
    return size - available_or_flag_word;
}

int dovi_dm_ParseMds(const unsigned char *input, unsigned int size, unsigned int *metadata,
                     int input_index, const void *configuration, long context)
{
    struct dm_metadata_format *decoded = (void *)metadata;
    unsigned int available, paired_available, index;
    unsigned short diagonal;

    decoded->flags = 0;
    decoded->ycc_to_rgb_scale = 13;
    decoded->rgb_to_lms_scale = 14;
    if ((int)((const struct dm_control *)configuration)->primary_signal == input_index) {
        *(char *)(context + 0x16ec) = 0;
        *(unsigned int *)(context + 0x16e8) = 0;
        *(unsigned int *)(context + 0x4398) = 0;
        *(long *)(context + 0x36f0) = context + 0x26ed;
    }
    if (size == 0)
        return -1;
    decoded->version_major = input[0];
    if (size == 1)
        return -1;
    decoded->version_minor = input[1];
    if (fixed_signed32(size - 2u) < 18)
        return -1;
    for (index = 0; index < 9; ++index)
        decoded->ycc_to_rgb[index] = (unsigned short)fixed_load_be16(input + 2 + index * 2);

    /* These unsigned checkpoints retain the native large-size behavior. */
    if (size - 0x20u >= 0xfffffff2u)
        return -1;
    for (index = 0; index < 3; ++index)
        decoded->ycc_offsets[index] = fixed_load_be32(input + 0x14 + index * 4);
    available = size - 0x32u;
    if (available >= 0xffffffeau)
        return -1;
    for (index = 0; index < 9; ++index)
        decoded->rgb_to_lms[index] = (unsigned short)fixed_load_be16(input + 0x20 + index * 2);

    if ((size & 0xfffffffcu) == 0x30u)
        return -1;
    paired_available = available & 0xfffffffeu;
    decoded->eotf = (unsigned short)fixed_load_be16(input + 0x32);
    if (paired_available == 2)
        return -1;
    decoded->eotf_scale = (unsigned short)fixed_load_be16(input + 0x34);
    if (paired_available == 4)
        return -1;
    decoded->eotf_offset = (unsigned short)fixed_load_be16(input + 0x36);
    if (size - 0x3cu >= 0xfffffffcu)
        return -1;
    decoded->eotf_range = fixed_load_be32(input + 0x38);
    if (available == 10)
        return -1;
    decoded->bit_depth = input[0x3c];
    if (available == 11)
        return -1;
    decoded->color_space = input[0x3d];
    if (available == 12)
        return -1;
    decoded->chroma_format = input[0x3e];
    if (available == 13)
        return -1;
    decoded->full_range = input[0x3f];
    if (paired_available == 14)
        return -1;
    decoded->source_min_pq = (unsigned short)fixed_load_be16(input + 0x40);
    if (paired_available == 16)
        return -1;
    decoded->source_max_pq = (unsigned short)fixed_load_be16(input + 0x42);
    if (paired_available == 18)
        return -1;
    /* Read before resetting counters: input and destination may overlap. */
    diagonal = (unsigned short)fixed_load_be16(input + 0x44);
    metadata[0x23] = 0;
    metadata[0x46] = 0;
    metadata[0x88] = 0;
    decoded->flags |= 1u;
    decoded->source_diagonal = diagonal;
    if (available != 0x14u)
        return parse_metadata_extensions(input, size, metadata, input_index, configuration,
                                         context);
    return -1;
}
