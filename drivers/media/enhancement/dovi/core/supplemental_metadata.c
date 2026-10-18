/* Supplemental metadata decoding. */
#include "fixed_access.h"
#include "parser_access.h"
#include "supplemental_metadata.h"
#include "luminance_pq.h"
#include "bitstream.h"
#include "align_to_byte.h"
#include "ext_blocks.h"
#include "parser_api.h"

typedef char require_abi[(sizeof(void *) == 8 && sizeof(unsigned int) == 4 &&
                          sizeof(unsigned long long) == 8 && sizeof(int) == 4)
                             ? 1
                             : -1];

static void synthetic_mapping(unsigned char *header, unsigned char *payload)
{
    unsigned int component;
    header[0x9] = 0;
    header[0xf] = 10;
    header[0xa] = 23;
    header[0x10] = 8;
    header[0x11] = 12;
    header[0xf0] = 0;
    header[0x133] = 1;
    header[0x134] = 1;
    header[6] = 1;
    header[0x16] = 1;
    header[0x15] = 0;
    header[0x12] = 0;
    for (component = 0; component < 3; ++component) {
        unsigned char *record = payload + 0x5c + component * 0x30c;
        header[0x25 + component] = 2;
        fixed_store_le16(header + 0x88 + component * 32u, 0);
        fixed_store_le16(header + 0x8a + component * 32u, (1u << (header[0xf] & 31u)) - 1u);
        payload[component * 15u] = 0;
        record[0] = 1;
        fixed_store_le32(record + 0xc, 0);
        fixed_store_le32(record + 0x20, 0);
        fixed_store_le32(record + 0x10, 1);
        fixed_store_le32(record + 0x24, 0x1806);
    }
}

int dovi_st2094_10_data_read(void *opaque)
{
    unsigned char *decoder = opaque;
    struct dovi_bitstream *bits = (struct dovi_bitstream *)(decoder + 0x40);
    unsigned char *root = parser_load_pointer(decoder);
    unsigned char *metadata = parser_load_pointer(root + 0x10);
    unsigned char *header = parser_load_pointer(root);
    unsigned char *payload = parser_load_pointer(root + 0x8);
    unsigned int initial = bits->bits_read;
    unsigned int country, provider, identifier;
    fixed_store_le32(decoder + 0x315b8, 0);
    country = dovi_read_u_v(8, bits);
    if (country == 255) {
        (void)dovi_read_u_v(8, bits);
        return 0;
    }
    if (country != 0xb5)
        return 0;
    provider = dovi_read_u_v(8, bits) << 8;
    provider |= dovi_read_u_v(8, bits);
    if (provider != 0x3b && provider != 0x31)
        return 0;
    identifier = dovi_read_u_v(32, bits);
    if (identifier != 0 && identifier != 0x47413934u)
        return 0;
    if (dovi_read_u_v(8, bits) != 9)
        return 0;
    fixed_store_le32(decoder + 0x315b8, 1);
    (void)dovi_read_ue_v(bits);
    (void)dovi_read_ue_v(bits);
    if (parser_read_flag(bits) != 0) {
        metadata[0x4a] = (unsigned char)dovi_read_ue_v(bits);
        align_to_byte(bits);
        if (metadata[0x4a] != 0) {
            unsigned int index = 0, count, scan;
            int has_level8 = 0, has_level254 = 0;
            unsigned char current_index = 0;
            do {
                unsigned char *record = metadata + 0x4c + index * 0x108u;
                int result;
                fixed_store_le32(record, dovi_read_ue_v(bits));
                record[4] = (unsigned char)dovi_read_u_v(8, bits);
#if PARSER_DECODER_EXTENSION_API
                result = dovi_ext_block_payload_read(decoder, record, bits, &current_index);
#else
                result = dovi_ext_block_payload_read(parser_load_pointer(decoder + 8),
                                                      decoder + 0x316d8, record, bits, &current_index);
#endif
                if (record[4] == 3) {
                    fixed_store_le16(record + 6, fixed_load_le16(record + 6) >> 1);
                    fixed_store_le16(record + 0x8, fixed_load_le16(record + 0x8) >> 1);
                    fixed_store_le16(record + 0xa, fixed_load_le16(record + 0xa) >> 1);
                } else if (record[4] == 8) {
                    has_level8 = 1;
                } else if (record[4] == 254) {
                    fixed_store_le16(record + 6, 0x200);
                    has_level254 = 1;
                }
                count = metadata[0x4a];
                ++index;
                initial += (unsigned int)result;
                current_index = (unsigned char)index;
            } while (index < count);
            if (!has_level8 && metadata[0x4a] != 0) {
                scan = 0;
                do {
                    unsigned char *record = metadata + 0x4c + scan * 0x108u;
                    if (record[4] == 2) {
                        unsigned int target = fixed_load_le16(record + 6);
                        record[4] = 8;
                        fixed_store_le32(record, 10);
                        if (target >= 0x81cu && target <= 0x825u)
                            record[6] = 1;
                        else if (target >= 0xb1fu && target <= 0xb28u)
                            record[6] = 27;
                        else if (target >= 0xc02u && target <= 0xc0bu)
                            record[6] = 48;
                        else if (target >= 0xd38u && target <= 0xd41u)
                            record[6] = 37;
                        else {
                            unsigned char *extra = metadata + 0x4c + (index & 255u) * 0x108u;
                            ++count;
                            ++index;
                            fixed_store_le32(extra, 5);
                            extra[4] = 10;
                            extra[6] = 255;
                            fixed_store_le16(extra + 0x8, fixed_load_le16(record + 6));
                            fixed_store_le16(extra + 0xa, 62);
                            extra[0xc] = 0;
                            record[6] = 255;
                            metadata[0x4a] = (unsigned char)count;
                        }
                        if (fixed_load_le16(record + 0x12) == 0xffffu)
                            fixed_store_le16(record + 0x12, 0x800);
                    }
                    ++scan;
                } while (scan < (count & 255u));
            }
            if (!has_level254) {
                unsigned char *extra = metadata + 0x4c + (index & 255u) * 0x108u;
                metadata[0x4a] = (unsigned char)(metadata[0x4a] + 1u);
                fixed_store_le32(extra, 2);
                extra[4] = 254;
                fixed_store_le16(extra + 6, 0x200);
            }
        }
    }
    synthetic_mapping(header, payload);
    return fixed_signed32(bits->bits_read - initial);
}

int dovi_mastering_display_colour_volume_read(void *opaque)
{
    unsigned char *decoder = opaque;
    unsigned char *metadata = parser_load_pointer(parser_load_pointer(decoder) + 0x10);
    struct dovi_bitstream *bits = (struct dovi_bitstream *)(decoder + 0x40);
    unsigned int coordinate, maximum, minimum;
    /* The eight primary/white-point coordinates are consumed, not stored. */
    for (coordinate = 0; coordinate < 8; ++coordinate)
        (void)dovi_read_u_v(16, bits);
    maximum = dovi_read_u_v(32, bits);
    minimum = dovi_read_u_v(32, bits);
    /* Use a 32-bit shift for minimum luminance and 64 bits for maximum. */
    fixed_store_le16(metadata + 0x44, dovi_luminance_to_pq12((minimum << 18) / 10000u));
    fixed_store_le16(
        metadata + 0x46,
        dovi_luminance_to_pq12((unsigned int)(((unsigned long long)maximum << 18) / 10000u)));
    return fixed_signed32(bits->bits_read);
}
