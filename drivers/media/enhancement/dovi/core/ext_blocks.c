/* Parse extension headers and level-specific metadata payloads. */
#include "parser_api.h"
#include "dm_payload.h"
#include "bitstream.h"
#include "decoder_lifecycle.h"

typedef unsigned char byte;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef int (*log_fn)(const char *, ...);
typedef dec_event_callback_t event_fn;
typedef char require_uint32[(sizeof(u32) == 4 && sizeof(int) == 4) ? 1 : -1];
typedef char require_pointer64[(sizeof(void *) == 8 && sizeof(log_fn) == 8 && sizeof(event_fn) == 8)
                                   ? 1
                                   : -1];

static void copy8(byte *dest, const byte *src)
{
    u32 i;
    for (i = 0; i < 8u; ++i)
        dest[i] = src[i];
}

static byte *read_pointer(const byte *p)
{
    byte *value;
    copy8((byte *)&value, p);
    return value;
}

static log_fn read_log(const byte *p)
{
    log_fn value;
    copy8((byte *)&value, p);
    return value;
}

static event_fn read_event(const byte *p)
{
    event_fn value;
    copy8((byte *)&value, p);
    return value;
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
    write16(p, value);
    write16(p + 2, value >> 16);
}

static int signed32(u32 value)
{
    return (value & 0x80000000u) ? -1 - (int)~value : (int)value;
}

static void zero_message(char *message)
{
    volatile char *dest = message;
    u32 i;
    for (i = 0; i < 256u; ++i)
        dest[i] = 0;
}

/* Each diagnostic substitutes one decimal value in 0..255. */
#include "message_decimal_impl.inc"

static void read_words(byte *dest, u32 count, u32 width, struct dovi_bitstream *bits)
{
    while (count-- != 0) {
        write16(dest, dovi_read_u_v(width, bits));
        dest += 2;
    }
}

static void read_bytes(byte *dest, u32 count, struct dovi_bitstream *bits)
{
    while (count-- != 0)
        *dest++ = (byte)dovi_read_u_v(8u, bits);
}

#if PARSER_DECODER_EXTENSION_API
int dovi_ext_block_payload_read(void *decoder, void *block, void *dovi_bitstream,
                                const byte *block_index)
{
    byte *ctx = (byte *)decoder;
    byte *record = (byte *)block;
    byte *config = read_pointer(ctx + 8);
    const byte *event_slot = ctx + 0x316d8;
    struct dovi_bitstream *bits = (struct dovi_bitstream *)dovi_bitstream;
#else
int dovi_ext_block_payload_read(void *context, void *event_slot, void *block,
                                struct dovi_bitstream *bits, const byte *block_index)
{
    byte *record = (byte *)block;
    byte *config = (byte *)context;
#endif
    u32 length = read32(record), declared_bits = length * 8u;
    u32 consumed = 0, level = record[4], limit = 0, i;
    int result = 0;
    const char *limit_message = (const char *)0;
    char message[256];
    zero_message(message);

    switch (level) {
    case 1:
    case 3:
        read_words(record + 6, 3, 12, bits);
        consumed = 36;
        break;
    case 2:
        read_words(record + 6, 6, 12, bits);
        {
            u32 value = dovi_read_u_v(13, bits);
            write16(record + 0x12, value | (0u - (value & 0x1000u)));
        }
        consumed = 85;
        limit = 8;
        limit_message = "Found more than %d L2 trims\n";
        break;
    case 4:
        read_words(record + 6, 2, 12, bits);
        consumed = 24;
        break;
    case 5:
        read_words(record + 6, 4, 13, bits);
        consumed = 52;
        break;
    case 6:
        read_words(record + 6, 4, 16, bits);
        consumed = 64;
        break;
    case 8:
        record[6] = (byte)dovi_read_u_v(8, bits);
        read_words(record + 8, 6, 12, bits);
        consumed = 80;
        if (read32(record) >= 11u) {
            read_words(record + 0x14, 1, 12, bits);
            consumed = 92;
            if (read32(record) >= 13u) {
                read_words(record + 0x16, 1, 12, bits);
                consumed = 104;
                if (read32(record) >= 14u) {
                    read_bytes(record + 0x18, 6, bits);
                    consumed = 152;
                    if (read32(record) >= 20u) {
                        read_bytes(record + 0x1e, 6, bits);
                        consumed = 200;
                    }
                }
            }
        }
        limit = 5;
        limit_message = "Found more than %d L8 trims\n";
        break;
    case 9:
        record[6] = (byte)dovi_read_u_v(8, bits);
        consumed = 8;
        if (record[6] >= 19u) {
            /* Signed 16-bit extraction has identical stored low 16 bits. */
            read_words(record + 8, 8, 16, bits);
            consumed = 136;
        }
        break;
    case 10:
        record[6] = (byte)dovi_read_u_v(8, bits);
        read_words(record + 8, 2, 12, bits);
        record[0x0c] = (byte)dovi_read_u_v(8, bits);
        consumed = 40;
        if (record[0x0c] >= 19u) {
            read_words(record + 0x0e, 8, 16, bits);
            consumed = 168;
        }
        limit = 4;
        limit_message = "Found more than %d L10 trims\n";
        break;
    case 11:
        read_bytes(record + 6, 4, bits);
        consumed = 32;
        break;
    case 254:
        read_bytes(record + 6, 2, bits);
        consumed = 16;
        break;
    case 255:
        read_bytes(record + 6, 6, bits);
        consumed = 48;
        break;
    default:
        if (length != 0) {
            i = 0;
            do {
                record[6 + i] = (byte)dovi_read_u_v(8, bits);
                consumed += 8u;
#if PARSER_BOUNDED_UNKNOWN_EXTENSION
                if (i == 255u)
                    break;
#endif
                ++i;
            } while (i < read32(record));
        }
        break;
    }

    if (limit != 0 && config[0x67c + record[4]] >= limit) {
        if (read_event((const byte *)event_slot) != (event_fn)0) {
            message_decimal(message, limit_message, limit);
            read_event((const byte *)event_slot)(4, 0x100, message);
        }
        result = -1;
    } else {
        config[0x77c + record[4]] = *block_index;
        config[0x67c + record[4]] = (byte)(config[0x67c + record[4]] + 1u);
    }

    /* Minimum payload lengths are not enforced here. */
    if (consumed < declared_bits) {
        u32 remaining = declared_bits - consumed;
        do {
            (void)parser_read_flag(bits);
        } while (--remaining != 0);
    }
    return result;
}

#include "extension_levels_impl.inc"

int dovi_ext_blocks_read(void *decoder, void *blocks, byte *count, u32 kind)
{
    byte *ctx = (byte *)decoder, *records = (byte *)blocks;
    struct dovi_bitstream *bits = (struct dovi_bitstream *)(ctx + 0x40);
    log_fn log = read_log(ctx + 0x315c8);
    u32 log_level = read32(ctx + 0x315d0), adjusted_count = *count;
    u32 output_count = 0, processed = 0;
    byte block_index = 0;
    char message[256];
    zero_message(message);

    if (*count != 0) {
        align_to_byte(bits);

        if (*count != 0) {
            do {
                u32 length = dovi_read_ue_v(bits);
                u32 level = dovi_read_u_v(8, bits);
                u32 selected;
                byte *config;
                u64 remaining;
                int payload_result;

                if (read_event(ctx + 0x316d8) != (event_fn)0) {
                    int old_payload_level = level == 1u || level == 2u || level == 4u ||
                                            level == 5u || level == 6u || level == 255u;
                    if ((kind == 1u && !old_payload_level) ||
                        ((PARSER_NONFIRST_EXTENSION_KIND ? kind != 1u : kind == 2u) &&
                         old_payload_level)) {
                        message_decimal(message,
                                        kind == 1u ? "Found level %d extension in payload 1"
                                                   : "Found level %d extension in payload 2",
                                        level);
                        read_event(ctx + 0x316d8)(4, 0x105, message);
                    }
                }

                config = read_pointer(ctx + 8);
                if (unique_level(level) && config[0x67c + level] != 0) {
                    --adjusted_count;
                    block_index = config[0x77c + level];
                    selected = block_index;
                    if (read_event(ctx + 0x316d8) != (event_fn)0) {
                        message_decimal(message, "Found more than one L%d extension level", level);
                        read_event(ctx + 0x316d8)(4, 0x101, message);
                    }
                } else {
                    byte *record = records + (u64)(output_count & 255u) * 0x108;
                    block_index = (byte)output_count;
                    selected = output_count;
                    output_count += 1u;
                    write32(record, length);
                    record[4] = (byte)level;
                }

                if (length > 256u) {
                    if (log != (log_fn)0 && log_level >= 3u)
                        log("ERROR: Found level %d extension with length %d (> MAX_UNKNOWN_MD_SIZE %d)\n",
                            level, signed32(length), 256);
                    return -2;
                }
                remaining = (u64)bits->tail - (u64)bits->next + 1u;
                if ((remaining & 0x8000000000000000ull) != 0 || remaining < length) {
                    if (log != (log_fn)0 && log_level >= 3u)
                        log("ERROR: Found level %d extension with length %d. This is beyond the size of the rpu input buffer. The stream is corrupted.\n",
                            level, length, 256);
                    return -2;
                }
#if PARSER_DECODER_EXTENSION_API
                payload_result = dovi_ext_block_payload_read(
                    ctx, records + (u64)(selected & 255u) * 0x108, bits, &block_index);
#else
                payload_result = dovi_ext_block_payload_read(
                    read_pointer(ctx + 8), ctx + 0x316d8, records + (u64)(selected & 255u) * 0x108,
                    bits, &block_index);
#endif
                if (payload_result < 0) {
                    output_count -= 1u;
                    --adjusted_count;
                }
                ++processed;
            } while (processed < *count);
        }
    }
    if ((output_count & 255u) != 255u)
        records[(u64)(output_count & 255u) * 0x108 + 4] = 0;
    *count = (byte)adjusted_count;
    return 0;
}
