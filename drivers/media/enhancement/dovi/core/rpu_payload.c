/* RPU payload decoding. */
#include "parser_api.h"
#include "rpu_payload.h"
#include "bitstream.h"
#include "resampling_filters.h"
#include "pivot_values.h"
#include "nlq_layout.h"

typedef unsigned char byte;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef int (*log_fn)(const char *, ...);
typedef char require_uint32[(sizeof(u32) == 4 && sizeof(int) == 4) ? 1 : -1];
typedef char require_pointer64[(sizeof(void *) == 8 && sizeof(log_fn) == 8 && sizeof(u64) == 8 &&
                                sizeof(long long) == 8)
                                   ? 1
                                   : -1];

static u32 read16(const byte *p)
{
    return (u32)p[0] | ((u32)p[1] << 8);
}

static u32 read32(const byte *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static void write16(byte *p, u32 value)
{
    p[0] = (byte)value;
    p[1] = (byte)(value >> 8);
}

static void write32(byte *p, u32 value)
{
    p[0] = (byte)value;
    p[1] = (byte)(value >> 8);
    p[2] = (byte)(value >> 16);
    p[3] = (byte)(value >> 24);
}

static void copy_pointer(byte *to, const byte *from)
{
    u32 i;
    for (i = 0; i < 8; ++i)
        to[i] = from[i];
}

static byte *pointer_at(const byte *p)
{
    byte *value;
    copy_pointer((byte *)&value, p);
    return value;
}

static log_fn logger_at(const byte *p)
{
    log_fn value;
    copy_pointer((byte *)&value, p);
    return value;
}

static int signed32(u32 value)
{
    return (value & 0x80000000u) ? -1 - (int)~value : (int)value;
}

static byte *partition_at(byte *payload, int partition)
{
    /* ARM64 SMADDL: sign-extend the 32-bit index before multiplying. */
    return (byte *)((u64)payload + (u64)((long long)partition * 0x7038));
}

static void error_line(log_fn log, u32 level, u32 line, int code)
{
    if (log != (log_fn)0 && level >= 3u)
        log("%s+%d: %s\n", PARSER_DIAGNOSTIC_SOURCE, line, dovi_rpu_error_code_2_str(code));
}

/* Stores each pair immediately; later fields may fault or trigger a trap. */
static void coefficient_pair(byte *integer, byte *fraction, byte *header, u32 width_offset,
                             struct dovi_bitstream *bits)
{
    write32(integer, (u32)dovi_read_se_v(bits));
    write32(fraction, dovi_read_u_v(header[width_offset], bits));
}

static void polynomial(byte *header, byte *record, u32 component, u32 pivot,
                       struct dovi_bitstream *bits)
{
    byte order_minus_one = (byte)dovi_read_ue_v(bits);
    u32 coefficient;
    record[0] = (byte)(order_minus_one + 1u);
    if (order_minus_one == 0) {
        record[1] = (byte)parser_read_flag(bits);
        if (record[0] == 1 && record[1] != 0) {
            coefficient_pair(record + 4, record + 8, header, 10, bits);
            if ((u32)header[0x25 + component] - 2u == pivot)
                coefficient_pair(record + 0x38, record + 0x3c, header, 10, bits);
            return;
        }
    }
    coefficient_pair(record + 0xc, record + 0x20, header, 10, bits);
    for (coefficient = 1;
         coefficient <= record[0] && (!PARSER_FATAL_ARRAY_LIMITS || coefficient <= 4u);
         ++coefficient)
        coefficient_pair(record + 0xc + coefficient * 4u, record + 0x20 + coefficient * 4u, header,
                         10, bits);
#if PARSER_FATAL_ARRAY_LIMITS
    if (record[0] > 4) {
        (void)dovi_read_se_v(bits);
        parser_limit_trap();
    }
#endif
}

static void multi_regression(byte *header, byte *record, struct dovi_bitstream *bits)
{
    u32 order, coefficient;
    record[0] = (byte)(dovi_read_u_v(2, bits) + 1u);
    coefficient_pair(record + 4, record + 8, header, 10, bits);
    for (order = 0; order < record[0]; ++order) {
        u32 value = (u32)dovi_read_se_v(bits);
#if PARSER_FATAL_ARRAY_LIMITS
        if (order == 3)
            parser_limit_trap();
#endif
        write32(record + 0xc + order * 0x1c, value);
        write32(record + 0x60 + order * 0x1c, dovi_read_u_v(header[10], bits));
        for (coefficient = 1; coefficient < 7; ++coefficient)
            coefficient_pair(record + 0xc + order * 0x1c + coefficient * 4u,
                             record + 0x60 + order * 0x1c + coefficient * 4u, header, 10, bits);
    }
}

static int mapping_read(byte *decoder, int partition, log_fn outer_log, u32 outer_level)
{
    byte *root = pointer_at(decoder);
    byte *header = pointer_at(root);
    byte *payload = pointer_at(root + 8);
    byte *config = pointer_at(decoder + 8);
    byte *part = partition_at(payload, partition);
    struct dovi_bitstream *bits = (struct dovi_bitstream *)(decoder + 0x40);
    log_fn log = logger_at(decoder + 0x315c8);
    u32 level = read32(decoder + 0x315d0);
    u32 component, pivot;
    int predicted_partition = 0;

    for (component = 0; component < 3; ++component) {
        byte *pivot_count = header + 0x25 + component;
        if (*pivot_count < 2)
            *pivot_count = 2;
        pivot = 0;
        do {
            byte mapping = (byte)dovi_read_ue_v(bits);
            byte *reuse;
            u32 coordinate = component * 0x3c + pivot * 4u;
#if PARSER_FATAL_ARRAY_LIMITS
            if (pivot == 15)
                parser_limit_trap();
#endif
            part[component * 15u + pivot] = mapping;
            reuse = part + 0x2d + component * 15u + pivot;
            if (mapping <= 3) {
                byte *count = config + mapping * 0xb4u + coordinate;
                byte *history = config + 0x39c + mapping * 0xb4u + coordinate;
                int use_prediction = 0;
                if (signed32(read32(count)) < 1) {
                    *reuse = 0;
                } else {
                    *reuse = (byte)parser_read_flag(bits);
                    use_prediction = *reuse != 0;
                }
                if (use_prediction) {
                    if (signed32(read32(count)) < 2) {
                        predicted_partition = signed32(read32(history));
                    } else {
                        byte distance = (byte)(dovi_read_ue_v(bits) + 1u);
                        predicted_partition = signed32((u32)partition - distance);
                        part[0x605c + component * 15u + pivot] = distance;
                    }
                    /* The original dereferences before its range check. */
                    write32(part + 0x6ea0 + coordinate,
                            read32(partition_at(pointer_at(root + 8), predicted_partition) +
                                   0x6ea0 + coordinate));
                } else {
                    byte *current_root = pointer_at(decoder);
                    byte *current_header = pointer_at(current_root);
                    byte *current_payload = pointer_at(current_root + 8);
                    byte *current_part = partition_at(current_payload, partition);
                    u32 record_offset;
                    if (mapping == 0) {
                        record_offset = 0x5c + component * 0x30c + pivot * 0x34;
                        polynomial(current_header, current_part + record_offset, component, pivot,
                                   bits);
                    } else if (mapping == 1) {
                        record_offset = 0x5c + component * 0xa8c + pivot * 0xb4;
                        multi_regression(current_header, current_part + record_offset, bits);
                    } else if (mapping == 2) {
                        u32 coefficient;
                        record_offset = 0x5c + component * 0x258 + pivot * 0x28;
                        for (coefficient = 0; coefficient < 5; ++coefficient)
                            coefficient_pair(current_part + record_offset + coefficient * 4u,
                                             current_part + record_offset + 0x14 + coefficient * 4u,
                                             current_header, 10, bits);
                    } else {
                        u32 first = read16(current_header + 0x88 + component * 0x20 + pivot * 2u);
                        u32 end = read16(current_header + 0x8a + component * 0x20 + pivot * 2u);
                        u32 entry;
                        if ((u32)current_header[0x25 + component] - 2u == pivot)
                            ++end;
                        if (end > 0x400) {
                            log_fn current_log = logger_at(decoder + 0x315c8);
                            if (current_log != (log_fn)0 && read32(decoder + 0x315d0) >= 3)
                                current_log(
                                    "ERROR: Partition %d, cmp %d, pivot_idx %d:  mapping LUT size is limited to %d but is %d\n",
                                    partition, component, pivot, 0x400, end);
                            error_line(log, level, 0x371, -2);
                            error_line(outer_log, outer_level, 0x3c1, -2);
                            return -2;
                        }
                        record_offset = 0x5c + component * 0x2000;
                        for (entry = first; entry < end; ++entry)
                            coefficient_pair(current_part + record_offset + entry * 4u,
                                             current_part + record_offset + 0x1000 + entry * 4u,
                                             current_header, 10, bits);
                    }
                    if (read32(history) == 0)
                        write32(history, (u32)partition);
                    write32(part + 0x6ea0 + coordinate, (u32)((u64)current_payload - (u64)payload) +
                                                            (u32)partition * 0x7038u +
                                                            record_offset);
                    write32(count, read32(count) + 1u);
                }
            }
            if (*reuse == 1 && (predicted_partition < 0 || predicted_partition >= partition)) {
                if (log != (log_fn)0 && level >= 3)
                    log("ERROR: Partition %d, cmp %d, pivot_idx %d:  pred_part_idx_mapping shall be in the range [0 .. %d] but is %d\n",
                        partition, component, pivot, partition - 1, predicted_partition);
                error_line(outer_log, outer_level, 0x3c1, -2);
                return -2;
            }
            ++pivot;
        } while ((long long)pivot < (long long)*pivot_count - 1);
    }
    return 0;
}

#include "rpu_chroma_impl.inc"

static void residual_filter_read(byte *decoder)
{
    byte *root = pointer_at(decoder);
    byte *header = pointer_at(root);
    byte *payload = pointer_at(root + 8);
    struct dovi_bitstream *bits = (struct dovi_bitstream *)(decoder + 0x40);
    u32 row, coefficient;
    write32(payload + 0x653c, dovi_read_u_v(header[0x11], bits));
    write32(payload + 0x6540, dovi_read_u_v(header[0x11], bits));
    if (header[0xe9] == 7) {
        for (row = 0; row < 3; ++row) {
            byte *current_root = pointer_at(decoder);
            byte *current_header = pointer_at(current_root);
            byte *current_payload = pointer_at(current_root + 8);
            for (coefficient = 0; coefficient < 8; ++coefficient)
                coefficient_pair(current_payload + 0x6544 + row * 0x20 + coefficient * 4u,
                                 current_payload + 0x66c4 + row * 0x20 + coefficient * 4u,
                                 current_header, 11, bits);
        }
    }
    if (header[0xed] == 7) {
        for (row = 0; row < 3; ++row) {
            byte mode = header[0xec];
            byte *current_root = pointer_at(decoder);
            byte *current_header = pointer_at(current_root);
            byte *current_payload = pointer_at(current_root + 8);
            for (coefficient = 0; coefficient < 6; ++coefficient)
                coefficient_pair(current_payload + 0x6604 + row * 0x20 + coefficient * 4u,
                                 current_payload + 0x6784 + row * 0x20 + coefficient * 4u,
                                 current_header, 11, bits);
            if (mode == 1) {
                for (coefficient = 0; coefficient < 6; ++coefficient)
                    coefficient_pair(current_payload + 0x6664 + row * 0x20 + coefficient * 4u,
                                     current_payload + 0x67e4 + row * 0x20 + coefficient * 4u,
                                     current_header, 11, bits);
            }
        }
    }

}

static int nlq_read(byte *decoder, int partition, log_fn outer_log, u32 outer_level)
{
    byte *root = pointer_at(decoder);
    byte *header = pointer_at(root);
    byte *payload = pointer_at(root + 8);
    byte *config = pointer_at(decoder + 8);
    byte *part = partition_at(payload, partition);
    struct dovi_bitstream *bits = (struct dovi_bitstream *)(decoder + 0x40);
    log_fn log = logger_at(decoder + 0x315c8);
    u32 level = read32(decoder + 0x315d0);
    u32 pivot = 0, component;
    decoder[0x316e0] = 1;
    if (header[0xf1] < 2)
        header[0xf1] = 2;
    do {
#if PARSER_FATAL_ARRAY_LIMITS
        if (pivot == 15)
            parser_limit_trap();
#endif
        for (component = 0; component < 3; ++component) {
            u32 coordinate = pivot * 3u + component;
            byte *count = config + 0x2e8 + coordinate * 4u;
            byte *reuse = part + DOVI_NLQ_PREDICTION_FLAGS_OFFSET + coordinate;
            byte *record =
                part + DOVI_NLQ_PARAMETERS_OFFSET + coordinate * DOVI_NLQ_PARAMETER_BYTES;
            byte *reference = part + DOVI_NLQ_COEFFICIENT_REFERENCES_OFFSET + coordinate * 4u;
            int use_prediction = 0;
            if (signed32(read32(count)) < 1) {
                *reuse = 0;
            } else {
                *reuse = (byte)parser_read_flag(bits);
                use_prediction = *reuse != 0;
            }
            if (use_prediction) {
                int predicted_partition;
                if (signed32(read32(count)) < 2) {
                    predicted_partition = 0;
                } else {
                    byte distance = (byte)(dovi_read_ue_v(bits) + 1u);
                    predicted_partition = signed32((u32)partition - distance);
                    reuse[DOVI_NLQ_PREDICTION_DISTANCE_OFFSET - DOVI_NLQ_PREDICTION_FLAGS_OFFSET] =
                        distance;
                }
                if (*reuse == 1 && (predicted_partition < 0 || predicted_partition >= partition)) {
                    if (log != (log_fn)0 && level >= 3)
                        log("ERROR: Partition %d, cmp %d, pivot_idx %d:  pred_part_idx_nlq shall be in the range [0 .. %d] but is %d\n",
#if PARSER_DECODER_EXTENSION_API
                            partition, component, pivot, partition - 1, predicted_partition);
#else
                            partition, component, pivot, partition - 1);
#endif
                    error_line(outer_log, outer_level, 0x3d0, -1);
                    return -1;
                }
                write32(reference,
                        read32(partition_at(pointer_at(root + 8), predicted_partition) +
                               DOVI_NLQ_COEFFICIENT_REFERENCES_OFFSET + coordinate * 4u));
            } else {
                u32 offset = dovi_read_u_v(header[0x10], bits);
                write16(record + DOVI_NLQ_OFFSET, offset);
                if ((offset & 0xffffu) != 0)
                    decoder[0x316e0] = 0;
                write32(record + DOVI_NLQ_VDR_IN_MAX_INT, dovi_read_ue_v(bits));
                write32(record + DOVI_NLQ_VDR_IN_MAX_FRACTION, dovi_read_u_v(header[10], bits));
                if (read32(record + DOVI_NLQ_VDR_IN_MAX_INT) != 1 ||
                    read32(record + DOVI_NLQ_VDR_IN_MAX_FRACTION) != 0)
                    decoder[0x316e0] = 0;
                if (header[0xf0] == 1) {
                    byte *current_root = pointer_at(decoder);
                    byte *current_header = pointer_at(current_root);
                    byte *current_part = partition_at(pointer_at(current_root + 8), partition);
                    write16(current_part + DOVI_NLQ_UNION_OFFSET +
                                coordinate * DOVI_NLQ_PARAMETER_BYTES,
                            dovi_read_u_v(current_header[0x10], bits));
                    write32(current_part + DOVI_NLQ_MU_LAW_MU_OFFSET +
                                coordinate * DOVI_NLQ_PARAMETER_BYTES,
                            dovi_read_ue_v(bits));
                } else if (header[0xf0] == 0) {
                    byte *current_root = pointer_at(decoder);
                    byte *current_header = pointer_at(current_root);
                    byte *current_record = partition_at(pointer_at(current_root + 8), partition) +
                                           DOVI_NLQ_UNION_OFFSET +
                                           coordinate * DOVI_NLQ_PARAMETER_BYTES;
                    write32(current_record, dovi_read_ue_v(bits));
                    write32(current_record + DOVI_NLQ_LINEAR_SLOPE_FRACTION -
                                DOVI_NLQ_LINEAR_SLOPE_INT,
                            dovi_read_u_v(current_header[10], bits));
                    write32(current_record + DOVI_NLQ_LINEAR_THRESHOLD_INT -
                                DOVI_NLQ_LINEAR_SLOPE_INT,
                            dovi_read_ue_v(bits));
                    write32(current_record + DOVI_NLQ_LINEAR_THRESHOLD_FRACTION -
                                DOVI_NLQ_LINEAR_SLOPE_INT,
                            dovi_read_u_v(current_header[10], bits));
                    if (read32(record + DOVI_NLQ_LINEAR_SLOPE_INT) != 0 ||
                        read32(record + DOVI_NLQ_LINEAR_SLOPE_FRACTION) != 0 ||
                        read32(record + DOVI_NLQ_LINEAR_THRESHOLD_INT) != 0 ||
                        read32(record + DOVI_NLQ_LINEAR_THRESHOLD_FRACTION) != 0)
                        decoder[0x316e0] = 0;
                }
                write32(reference, (u32)partition * DOVI_RPU_PAYLOAD_PARTITION_BYTES +
                                       DOVI_NLQ_PARAMETERS_OFFSET +
                                       coordinate * DOVI_NLQ_PARAMETER_BYTES);
                write32(count, read32(count) + 1u);
            }
        }
        ++pivot;
    } while ((long long)pivot < (long long)header[0xf1] - 1);
    return 0;
}

static void nlq_filter_read(byte *decoder)
{
    byte *root = pointer_at(decoder);
    byte *header = pointer_at(root);
    byte *payload = pointer_at(root + 8);
    struct dovi_bitstream *bits = (struct dovi_bitstream *)(decoder + 0x40);
    u32 group, coefficient;
    if (header[0x132] != 7)
        return;
    for (group = 0; group < 4; ++group) {
        u32 offset = 0x6d90 + group * 0x44;
        for (coefficient = 0; coefficient < 17; ++coefficient) {
            u32 width = header[0x17];
            u32 value = dovi_read_u_v(width + 2u, bits);
/* Sign extension wraps width+1 before selecting its one-bit sign mask.
             * The width and mask arithmetic retain their native 32-bit behavior. */
#if PARSER_NLQ_TRUNCATING_SIGN_SHIFT
            value |= 0u - ((2u << (width & 31u)) & value);
#else
            value |= 0u - ((1u << ((width + 1u) & 31u)) & value);
#endif
            write32(payload + offset + coefficient * 4u, value);
        }
        write32(pointer_at(root + 8) + 0x7028 + group * 4u, offset);
    }
}

int dovi_rpu_payload_read(void *opaque_decoder)
{
    byte *decoder = (byte *)opaque_decoder;
    byte *root = pointer_at(decoder);
    byte *header = pointer_at(root);
    byte *config = pointer_at(decoder + 8);
    struct dovi_bitstream *bits = (struct dovi_bitstream *)(decoder + 0x40);
    u32 initial_bits = bits->bits_read;
    log_fn log = logger_at(decoder + 0x315c8);
    u32 level = read32(decoder + 0x315d0);
    u32 i, row, column;
    int partition = 0, result;
    volatile byte *zero = config;
    for (i = 0; i < 0x39c; ++i)
        zero[i] = 0;
    if (pointer_at(root)[0] != 2 || pointer_at(root)[0x19] != 0)
        return signed32(bits->bits_read - initial_bits);
    result = dovi_assign_pivot_values((void *)root);
    if (result < 0)
        return result;
    if (header[0x16] == 0) {
        result = dovi_assign_nlq_pivot_values((void *)root);
        if (result < 0)
            return result;
    }
    for (row = 0; row < header[0x134]; ++row) {
        for (column = 0; column < header[0x133]; ++column) {
            result = mapping_read(decoder, partition, log, level);
            if (result < 0)
                return result;
            if ((read16(header + 2) & 0x700u) == 0 && header[0x1d] != 0 &&
                (header[0x1e] & 0xfeu) == 14) {
                result = chroma_read(decoder, partition, log, level);
                if (result < 0)
                    return result;
            }
            if (row == 0 && column == 0 && header[0x13] != 0 &&
                (header[0xe9] == 7 || header[0xed] == 7))
                residual_filter_read(decoder);
            if ((read16(header + 2) & 0x700u) == 0 && header[0x16] == 0) {
                result = nlq_read(decoder, partition, log, level);
                if (result < 0)
                    return result;
                if (row == 0 && column == 0 && header[0x1d] != 0)
                    nlq_filter_read(decoder);
            } else {
                decoder[0x316e0] = 0;
            }
            partition = signed32((u32)partition + 1u);
        }
    }
    return signed32(bits->bits_read - initial_bits);
}
