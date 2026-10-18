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
    int component = fixed_signed32(raw_component);
    int direction = fixed_signed32(raw_direction);
    unsigned char *decoder_bytes = decoder;
    unsigned char *root = parser_load_pointer(decoder_bytes);
    unsigned char *payload = parser_load_pointer(root + 8);
    unsigned char *rpu_context = parser_load_pointer(decoder_bytes + 8);
    struct dovi_bitstream *bits = (struct dovi_bitstream *)(decoder_bytes + 0x40);
    long long combination = (long long)component * 2 + direction;
    long long filter_offset = (long long)component * 0x50 + (long long)direction * 0x28;
    unsigned char *counter = rpu_context + 0x2d8 + combination * 4;
    unsigned char *partition_payload = payload + (long long)partition * 0x7038;
    unsigned char *flag = partition_payload + 0x649c + filter_offset;
    log_fn log = read_log(decoder_bytes + 0x315c8);
    unsigned int log_level = fixed_load_le32(decoder_bytes + 0x315d0);
    unsigned char *header;
    unsigned int coefficient_count, coefficient_index;

    if (fixed_signed32(fixed_load_le32(counter)) <= 0) {
        *flag = 0;
    } else {
        *flag = (unsigned char)dovi_read_u_v(1, bits);
        if (*flag != 0) {
            int predicted = 0;
            if (fixed_signed32(fixed_load_le32(counter)) >= 2) {
                unsigned char delta = (unsigned char)(dovi_read_ue_v(bits) + 1u);
                partition_payload[0x64c0 + filter_offset] = delta;
                predicted = fixed_signed32((unsigned int)partition - delta);
            }
            if (*flag == 1 && (predicted >= partition || predicted < 0)) {
                if (log != (log_fn)0 && log_level >= 3u)
                    log("ERROR: Partition %d, cmp %d, dir %d:  pred_part_idx_resampling1D shall be in the range [0 .. %d] but is %d\n",
                        partition, component, direction,
                        fixed_signed32((unsigned int)partition - 1u), predicted);
                return -2;
            }
            fixed_store_le32(partition_payload + 0x6f54 + combination * 4,
                             fixed_load_le32(payload + (long long)predicted * 0x7038 + 0x6f54 +
                                             combination * 4));
            return 0;
        }
    }

    /* Reload rpu_data/header after reading the prediction. */
    root = parser_load_pointer(decoder_bytes);
    header = parser_load_pointer(root);
    payload = parser_load_pointer(root + 8);
    coefficient_count = header[direction == 0 ? 0x20 : 0x1f];
    for (coefficient_index = 0; coefficient_index < coefficient_count; ++coefficient_index) {
        unsigned int width = header[0x0c];
        unsigned int value = dovi_read_u_v(width + 1u, bits);
        value |= 0u - ((1u << (width & 31u)) & value);
        fixed_store_le32(payload + (long long)partition * 0x7038 + 0x64a0 + filter_offset +
                             (long long)coefficient_index * 4,
                         value);
    }
    fixed_store_le32(partition_payload + 0x6f54 + combination * 4,
                     (unsigned int)(filter_offset + 0x64a0));
    fixed_store_le32(counter, fixed_load_le32(counter) + 1u);
    return 0;
}

int rpu_data_spatial_resampling_filter_exp_read(void *decoder, int pivot, int direction, int phases)
{
    unsigned char *decoder_bytes = decoder;
    unsigned char *root = parser_load_pointer(decoder_bytes);
    unsigned char *header = parser_load_pointer(root);
    unsigned char *payload = parser_load_pointer(root + 8);
    struct dovi_bitstream *bits = (struct dovi_bitstream *)(decoder_bytes + 0x40);
    unsigned int taps = direction == 0 ? 8u : 6u;
    int phase;
    unsigned int coefficient_index;
    long long filter_offset = 0x6544 + ((long long)direction * 6 + pivot) * 0x20;

    for (phase = 0; phase < phases; ++phase) {
        unsigned char *coefficients = payload + filter_offset + (long long)phase * 0x60;
        for (coefficient_index = 0; coefficient_index < taps; ++coefficient_index) {
            fixed_store_le32(coefficients + coefficient_index * 4u,
                             (unsigned int)dovi_read_se_v(bits));
            fixed_store_le32(coefficients + 0x180 + coefficient_index * 4u,
                             dovi_read_u_v(header[0x0b], bits));
        }
    }
    return 0;
}
