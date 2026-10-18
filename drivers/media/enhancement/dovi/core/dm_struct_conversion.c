/* Convert display metadata between native records and fixed packed records. */
#include "dm_serialization.h"

typedef unsigned char byte;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef char require_uint32[(sizeof(u32) == 4 && sizeof(int) == 4) ? 1 : -1];
typedef char require_pointer64[(sizeof(void *) == 8) ? 1 : -1];

static void copy_bytes(byte *dest, const byte *source, u32 count)
{
    while (count-- != 0)
        *dest++ = *source++;
}

static u32 read_le32(const byte *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static u32 read_be32(const byte *p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

static void write_le32(byte *p, u32 value)
{
    p[0] = (byte)value;
    p[1] = (byte)(value >> 8);
    p[2] = (byte)(value >> 16);
    p[3] = (byte)(value >> 24);
}

static void write_be32(byte *p, u32 value)
{
    p[0] = (byte)(value >> 24);
    p[1] = (byte)(value >> 16);
    p[2] = (byte)(value >> 8);
    p[3] = (byte)value;
}

static void swap_words(byte *dest, const byte *source, u32 count, u32 width)
{
    u32 i;
    while (count-- != 0) {
        for (i = 0; i < width; ++i)
            dest[i] = source[width - 1u - i];
        dest += width;
        source += width;
    }
}

static void pack_header(byte *dest, const byte *source)
{
    dest[0] = (byte)((u32)source[0] << 4 | source[1]);
    dest[1] = source[2];
    swap_words(dest + 0x02, source + 0x04, 9, 2);
    swap_words(dest + 0x14, source + 0x18, 3, 4);
    swap_words(dest + 0x20, source + 0x24, 12, 2);
    swap_words(dest + 0x38, source + 0x3c, 1, 4);
    copy_bytes(dest + 0x3c, source + 0x40, 4);
    swap_words(dest + 0x40, source + 0x44, 3, 2);
    dest[0x46] = source[0x4a];
}

static void unpack_header(byte *dest, const byte *source)
{
    dest[0] = source[0] >> 4;
    dest[1] = source[0] & 15u;
    dest[2] = source[1];
    swap_words(dest + 0x04, source + 0x02, 9, 2);
    swap_words(dest + 0x18, source + 0x14, 3, 4);
    swap_words(dest + 0x24, source + 0x20, 12, 2);
    swap_words(dest + 0x3c, source + 0x38, 1, 4);
    copy_bytes(dest + 0x40, source + 0x3c, 4);
    swap_words(dest + 0x44, source + 0x40, 3, 2);
    dest[0x4a] = source[0x46];
}

static int known_level(u32 level)
{
    return (level >= 1u && level <= 11u && level != 7u) || level >= 254u;
}

/* Return packed payload length; type and payload are written, not length. */
static u32 pack_record(byte *dest, const byte *source, int fixed_struct)
{
    u32 level = source[4], length = read_le32(source), result;
    dest[4] = (byte)level;
    switch (level) {
    case 1:
    case 3:
        swap_words(dest + 5, source + 6, 3, 2);
        return 6;
    case 2:
        swap_words(dest + 5, source + 6, 7, 2);
        return 14;
    case 4:
        swap_words(dest + 5, source + 6, 2, 2);
        return 4;
    case 5:
    case 6:
        swap_words(dest + 5, source + 6, 4, 2);
        return 8;
    case 8:
        dest[5] = source[6];
        swap_words(dest + 6, source + 8, 6, 2);
        result = 13;
        if (length >= 11u) {
            swap_words(dest + 18, source + 0x14, 1, 2);
            result = 15;
            if (length >= 13u) {
                swap_words(dest + 20, source + 0x16, 1, 2);
                result = 17;
                if (length >= 14u) {
                    copy_bytes(dest + 22, source + 0x18, 6);
                    result = 23;
                    if (length >= 20u) {
                        copy_bytes(dest + 28, source + 0x1e, 6);
                        result = 29;
                    }
                }
            }
        }
        return result;
    case 9:
        dest[5] = source[6];
        if (source[6] >= 19u) {
            swap_words(dest + 6, source + 8, 8, 2);
            return 17;
        }
        return 1;
    case 10:
        dest[5] = source[6];
        swap_words(dest + 6, source + 8, 2, 2);
        dest[10] = source[0x0c];
        result = fixed_struct ? 5u : 6u;
        if (source[0x0c] >= 19u) {
            swap_words(dest + 11, source + 0x0e, 8, 2);
            result += 16u;
        }
        return result;
    case 11:
        copy_bytes(dest + 5, source + 6, 4);
        return 4;
    case 254:
        copy_bytes(dest + 5, source + 6, 2);
        return 2;
    case 255:
        copy_bytes(dest + 5, source + 6, 6);
        return 6;
    default:
        copy_bytes(dest + 5, source + 6, length);
        return length;
    }
}

void dovi_dm_rpu_payload_2_struct(const void *payload, void *packed)
{
    const byte *source = (const byte *)payload;
    byte *dest = (byte *)packed;
    u32 input_index, output_index = 0;
    pack_header(dest, source);
    for (input_index = 0; input_index < source[0x4a]; ++input_index) {
        const byte *record = source + 0x4c + input_index * 0x108u;
        byte *target = dest + 0x47 + output_index * 0x105u;
        if (known_level(record[4])) {
            u32 length = pack_record(target, record, 1);
            write_be32(target, length);
            ++output_index;
        } else {
            /* Original omits both type write and output-index increment. */
            u32 length = read_le32(record);
            write_be32(target, length);
            copy_bytes(target + 5, record + 6, length);
        }
    }
}

void dovi_dm_struct_2_rpu_payload(void *payload, const void *packed)
{
    byte *dest = (byte *)payload;
    const byte *source = (const byte *)packed;
    u32 i;
    unpack_header(dest, source);
    for (i = 0; i < dest[0x4a]; ++i) {
        const byte *record = source + 0x47 + i * 0x105u;
        byte *target = dest + 0x4c + i * 0x108u;
        u32 level = record[4], length = read_be32(record);
        target[4] = (byte)level;
        switch (level) {
        case 1:
        case 3:
            write_le32(target, 5);
            swap_words(target + 6, record + 5, 3, 2);
            break;
        case 2:
            write_le32(target, 11);
            swap_words(target + 6, record + 5, 7, 2);
            break;
        case 4:
            write_le32(target, 3);
            swap_words(target + 6, record + 5, 2, 2);
            break;
        case 5:
        case 6:
            write_le32(target, level == 5u ? 7u : 8u);
            swap_words(target + 6, record + 5, 4, 2);
            break;
        case 8:
            write_le32(target, 10);
            target[6] = record[5];
            swap_words(target + 8, record + 6, 6, 2);
            if (length > 13u) {
                write_le32(target, 12);
                swap_words(target + 0x14, record + 18, 1, 2);
                if (length > 15u) {
                    write_le32(target, 13);
                    swap_words(target + 0x16, record + 20, 1, 2);
                    if (length > 17u) {
                        write_le32(target, 19);
                        copy_bytes(target + 0x18, record + 22, 6);
                        if (length > 23u) {
                            write_le32(target, 25);
                            copy_bytes(target + 0x1e, record + 28, 6);
                        }
                    }
                }
            }
            break;
        case 9:
            write_le32(target, 1);
            target[6] = record[5];
            if (record[5] >= 19u) {
                write_le32(target, 17);
                swap_words(target + 8, record + 6, 8, 2);
            }
            break;
        case 10:
            write_le32(target, 6);
            target[6] = record[5];
            swap_words(target + 8, record + 6, 2, 2);
            target[0x0c] = record[10];
            if (record[10] >= 19u) {
                write_le32(target, 22);
                swap_words(target + 0x0e, record + 11, 8, 2);
            }
            break;
        case 11:
            write_le32(target, 4);
            copy_bytes(target + 6, record + 5, 4);
            break;
        case 254:
            write_le32(target, 2);
            copy_bytes(target + 6, record + 5, 2);
            break;
        case 255:
            write_le32(target, 6);
            copy_bytes(target + 6, record + 5, 6);
            break;
        default:
            write_le32(target, length);
            copy_bytes(target + 6, record + 5, length);
            break;
        }
    }
    if (i < 255u)
        dest[0x50 + i * 0x108u] = 0;
}
