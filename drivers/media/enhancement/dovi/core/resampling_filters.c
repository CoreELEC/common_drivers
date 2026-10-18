/* Read spatial resampling filters and their prediction parameters. */
#include "fixed_access.h"
#include "resampling_filters.h"
#include "parser_access.h"
#include "bitstream.h"

typedef int (*log_fn)(const char *format, ...);
typedef char require_uint32[(sizeof(unsigned int) == 4) ? 1 : -1];
typedef char require_int32[(sizeof(int) == 4) ? 1 : -1];
typedef char require_offset64[(sizeof(long long) == 8) ? 1 : -1];
typedef char require_pointer64[(sizeof(void *) == 8 && sizeof(log_fn) == 8) ? 1 : -1];

static log_fn read_log(const unsigned char *source)
{
    log_fn value;
    parser_copy8(&value, source);
    return value;
}

int dovi_rpu_data_chroma_resampling_filter_1D_exp_read(void *decoder, int partition,
                                                       unsigned int raw_component,
                                                       unsigned int raw_direction)
{
    unsigned int component = raw_component;
    unsigned int direction = raw_direction;
    unsigned char *decoder_bytes = decoder;
    unsigned char *root = parser_load_pointer(decoder_bytes);
    unsigned char *payload = parser_load_pointer(root + 8);
    unsigned char *rpu_context = parser_load_pointer(decoder_bytes + 8);
    unsigned char *counter =
        rpu_context + (unsigned long long)component * 8 + (unsigned long long)direction * 4 + 0x2d8;
    long long partition_offset = (long long)partition * 0x7038;
    unsigned long long filter_offset =
        (unsigned long long)component * 0x50 + (unsigned long long)direction * 0x28;
    unsigned long long index_offset =
        (unsigned long long)component * 8 + (unsigned long long)direction * 4 + 0x6f54;
    unsigned char *flag = payload + partition_offset + filter_offset + 0x649c;
    struct dovi_bitstream *bits = (struct dovi_bitstream *)(decoder_bytes + 0x40);
    log_fn log = (log_fn)0;
    unsigned int log_level = 0, predicted = 0;

    if (fixed_signed32(fixed_load_le32(counter)) <= 0) {
        *flag = 0;
    } else {
        unsigned int prediction = dovi_read_u_1(bits);
        log = read_log(decoder_bytes + 0x315c8);
        log_level = fixed_load_le32(decoder_bytes + 0x315d0);
        *flag = (unsigned char)prediction;
        if (prediction != 0) {
            if (fixed_signed32(fixed_load_le32(counter)) >= 2) {
                unsigned int delta = (dovi_read_ue_v(bits) + 1u) & 255u;
                unsigned char saved_flag = *flag;
                predicted = (unsigned int)partition - delta;
                payload[partition_offset + filter_offset + 0x64c0] = (unsigned char)delta;
                if (saved_flag != 1)
                    goto copy_prediction;
            }
            if (fixed_signed32(predicted) < 0 || fixed_signed32(predicted) >= partition) {
                if (log != (log_fn)0 && log_level >= 3u) {
                    log("ERROR: Partition %d, cmp %d, dir %d:  pred_part_idx_resampling1D shall be in the range [0 .. %d] but is %d\n",
                        partition, component, direction,
                        fixed_signed32((unsigned int)partition - 1u), fixed_signed32(predicted));
                }
                return -2;
            }
        copy_prediction:
            fixed_store_le32(payload + partition_offset + index_offset,
                             fixed_load_le32(parser_load_pointer(root + 8) +
                                             (long long)fixed_signed32(predicted) * 0x7038 +
                                             index_offset));
            return 0;
        }
    }

    {
        /* Reload rpu_data/header after reading the prediction. */
        unsigned char *current_root = parser_load_pointer(decoder_bytes);
        unsigned char *header = parser_load_pointer(current_root);
        unsigned int coefficient_count = header[direction == 0 ? 0x20 : 0x1f];
        if (coefficient_count != 0) {
            unsigned char *coefficients =
                parser_load_pointer(current_root + 8) + partition_offset + filter_offset + 0x64a0;
            do {
                unsigned int width = header[0x0c];
                unsigned int value = dovi_read_u_v(width + 1u, bits);
                value |= 0u - ((1u << (width & 31u)) & value);
                fixed_store_le32(coefficients, value);
                coefficients += 4;
            } while (--coefficient_count != 0);
        }
    }
    fixed_store_le32(payload + partition_offset + index_offset,
                     direction * 0x28u + component * 0x50u + 0x64a0u);
    fixed_store_le32(counter, fixed_load_le32(counter) + 1u);
    return 0;
}
