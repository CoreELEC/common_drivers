/* Serialize display metadata into a big-endian base and extension sequence. */
#include "parser_api.h"
#include "dm_serialization.h"
#include "parser_contract.h"

typedef char dm_unsigned_is_32[(sizeof(unsigned int) == 4) ? 1 : -1];
typedef char dm_int_is_32[(sizeof(int) == 4) ? 1 : -1];

static unsigned int dm_load32(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
}

static int dm_signed32(unsigned int value)
{
    return value <= 0x7fffffffu ? (int)value : -1 - (int)~value;
}

static void dm_pairs(unsigned char *dst, const unsigned char *src, unsigned int count)
{
    unsigned int index;
    for (index = 0; index < count; index += 2) {
        dst[index] = src[index + 1];
        dst[index + 1] = src[index];
    }
}

static void dm_reverse32(unsigned char *dst, const unsigned char *src)
{
    dst[0] = src[3];
    dst[1] = src[2];
    dst[2] = src[1];
    dst[3] = src[0];
}

static void dm_block_header(unsigned char *dst, unsigned int count, unsigned char level)
{
    dst[0] = (unsigned char)(count >> 24);
    dst[1] = (unsigned char)(count >> 16);
    dst[2] = (unsigned char)(count >> 8);
    dst[3] = (unsigned char)count;
    dst[4] = level;
}

int dovi_dm_rpu_payload_2_byte_sequence(const void *dm, void *output, int *length)
{
    const unsigned char *src = (const unsigned char *)dm;
    unsigned char *dst = (unsigned char *)output;
    unsigned int index, position = 71;
    if (*length < 71)
        return -12;
    dst[0] = (unsigned char)(src[1] | (src[0] << 4));
    dst[1] = src[2];
    dm_pairs(dst + 2, src + 4, 18);
    for (index = 0; index < 3; ++index)
        dm_reverse32(dst + 20 + index * 4, src + 24 + index * 4);
    dm_pairs(dst + 32, src + 36, 24);
    dm_reverse32(dst + 56, src + 60);
    for (index = 0; index < 4; ++index)
        dst[60 + index] = src[64 + index];
    dm_pairs(dst + 64, src + 68, 6);
    dst[70] = src[74];

    for (index = 0; index < src[74]; ++index) {
        const unsigned char *record = src + 76 + index * 264;
        const unsigned char *payload = record + 6;
        unsigned char *block = dst + dm_signed32(position);
        unsigned int input_length = dm_load32(record), count, offset;
        unsigned char level = record[4];
        switch (level) {
        case 1:
        case 3:
            count = 6;
            break;
        case 2:
            count = 14;
            break;
        case 4:
            count = 4;
            break;
        case 5:
        case 6:
            count = 8;
            break;
        case 8:
            if (*length < dm_signed32(position + 18))
                return -12;
            block[4] = level;
            block[5] = payload[0];
            dm_pairs(block + 6, payload + 2, 12);
            count = 13;
            if (input_length >= 11) {
                if (*length < dm_signed32(position + 20))
                    return -12;
                dm_pairs(block + 18, payload + 14, 2);
                count = 15;
                if (input_length >= 13) {
                    if (*length < dm_signed32(position + 22))
                        return -12;
                    dm_pairs(block + 20, payload + 16, 2);
                    count = 17;
                    if (input_length >= 14) {
                        if (*length < dm_signed32(position + 28))
                            return -12;
                        for (offset = 0; offset < 6; ++offset)
                            block[22 + offset] = payload[18 + offset];
                        count = 23;
                        if (input_length >= 20) {
                            if (*length < dm_signed32(position + 34))
                                return -12;
                            for (offset = 0; offset < 6; ++offset)
                                block[28 + offset] = payload[24 + offset];
                            count = 29;
                        }
                    }
                }
            }
            dm_block_header(block, count, level);
            position += count + 5;
            continue;
        case 9:
            if (*length < dm_signed32(position + 6))
                return -12;
            block[4] = level;
            block[5] = payload[0];
            count = 1;
            if (payload[0] >= 19) {
                if (*length < dm_signed32(position + 22))
                    return -12;
                dm_pairs(block + 6, payload + 2, 16);
                count = 17;
            }
            dm_block_header(block, count, level);
            position += count + 5;
            continue;
        case 10:
            if (*length < dm_signed32(position + 11))
                return -12;
            block[4] = level;
            block[5] = payload[0];
            dm_pairs(block + 6, payload + 2, 4);
            block[10] = payload[6];
            count = 6;
            if (payload[6] >= 19) {
                if (*length < dm_signed32(position + 27))
                    return -12;
                dm_pairs(block + 11, payload + 8, 16);
                count = 22;
            }
            dm_block_header(block, count, level);
            position += count + 5;
            continue;
        case 11:
            count = 4;
            break;
        case 254:
            count = 2;
            break;
        case 255:
            count = 6;
            break;
        default:
            count = input_length;
            if (*length < dm_signed32(position + count + 5))
                return -12;
            dm_block_header(block, count, level);
            for (offset = 0; offset < count; ++offset) {
#if PARSER_WRAPPING_SEQUENCE_OFFSET
                dst[dm_signed32(position + 5 + offset)] = payload[offset];
#else
                block[5 + offset] = payload[offset];
#endif
            }
            position += count + 5;
            continue;
        }
        if (*length < dm_signed32(position + count + 5))
            return -12;
        dm_block_header(block, count, level);
        if (level == 11 || level >= 254) {
            for (offset = 0; offset < count; ++offset)
                block[5 + offset] = payload[offset];
        } else {
            dm_pairs(block + 5, payload, count);
        }
        position += count + 5;
    }
    *length = dm_signed32(position);
    return 0;
}
