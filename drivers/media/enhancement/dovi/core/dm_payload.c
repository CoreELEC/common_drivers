/* Parse display metadata and maintain the decoder metadata state. */
#include "parser_api.h"
#include "dm_payload.h"
#include "bitstream.h"
#include "decoder_lifecycle.h"

typedef unsigned char byte;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef int (*log_fn)(const char *format, ...);
typedef dec_event_callback_t event_fn;
typedef char require_uint32[(sizeof(u32) == 4 && sizeof(int) == 4) ? 1 : -1];
typedef char require_uint64[(sizeof(u64) == 8) ? 1 : -1];
typedef char require_pointer64[(sizeof(void *) == 8 && sizeof(log_fn) == 8 && sizeof(event_fn) == 8)
                                   ? 1
                                   : -1];

static void copy_bytes(byte *dest, const byte *source, u32 count)
{
    while (count-- != 0)
        *dest++ = *source++;
}

static void zero_bytes(byte *dest, u32 count)
{
    volatile byte *p = dest;
    while (count-- != 0)
        *p++ = 0;
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

static void write64(byte *p, u64 value)
{
    write32(p, (u32)value);
    write32(p + 4, (u32)(value >> 32));
}

static byte *read_pointer(const byte *p)
{
    byte *value;
    copy_bytes((byte *)&value, p, 8);
    return value;
}

static log_fn read_log(const byte *p)
{
    log_fn value;
    copy_bytes((byte *)&value, p, 8);
    return value;
}

static event_fn read_event(const byte *p)
{
    event_fn value;
    copy_bytes((byte *)&value, p, 8);
    return value;
}

static int signed32(u32 value)
{
    return (value & 0x80000000u) ? -1 - (int)~value : (int)value;
}

static int more_extension_bytes(const struct dovi_bitstream *bits)
{
    u64 remaining = (u64)bits->tail - (u64)bits->next;
    /* SUB X followed by signed B.LT against 5, not pointer ordering. */
    return remaining >= 5u && remaining < 0x8000000000000000ull;
}

static void report_error(log_fn log, u32 level, int error, u32 line)
{
    if (log != 0 && level > 2u)
        log("%s+%d: %s\n", PARSER_DIAGNOSTIC_SOURCE, line, dovi_rpu_error_code_2_str(error));
}

#include "dm_defaults_impl.inc"

#if PARSER_DECODER_EXTENSION_API
static void store_md(void *decoder, const void *dm);
#endif

int dovi_dm_data_payload_read(void *decoder)
{
    byte *d = (byte *)decoder;
    byte *root = read_pointer(d);
    byte *dm = read_pointer(root + 0x10);
    byte *header = read_pointer(root);
    struct dovi_bitstream *bits = (struct dovi_bitstream *)(d + 0x40);
    log_fn log = read_log(d + 0x315c8);
    u32 log_level = read32(d + 0x315d0);
    u32 initial_bits = bits->bits_read;
    byte count = 0;
    u32 i;
    int result;

    /* The function reloads the config pointer on each iteration. */
    for (i = 0x67c; i < 0x77c; ++i)
        read_pointer(d + 8)[i] = 0;

    if (header[0x14] == 1u) {
        if (read32(d + 0x80) == 1u) {
            copy_bytes(dm, d + 0x84, DOVI_DM_PAYLOAD_SIZE);
        } else {
            event_fn event = read_event(d + 0x316d8);
            if (event != 0)
                event(
                    4, 0x102,
                    "reserved_zero_3bits is set to 1, but no previous dm_payload found, using default values.");
            default_dm_payload(dm);
        }
    }

    dm[0] = (byte)dovi_read_ue_v(bits);
    dm[1] = (byte)dovi_read_ue_v(bits);
    dm[2] = (byte)dovi_read_ue_v(bits);

    if (header[0x14] == 0u) {
        write32(d + 0x80, 1);
        /* Nine signed 16-bit values are stored as their low 16 bits. */
        for (i = 0x04; i <= 0x14; i += 2)
            write16(dm + i, dovi_read_u_v(16, bits));
        for (i = 0x18; i <= 0x20; i += 4)
            write32(dm + i, dovi_read_u_v(32, bits));
        /* Nine further signed values, then three unsigned values. */
        for (i = 0x24; i <= 0x3a; i += 2)
            write16(dm + i, dovi_read_u_v(16, bits));
        write32(dm + 0x3c, dovi_read_u_v(32, bits));
        dm[0x40] = (byte)dovi_read_u_v(5, bits);
        dm[0x41] = (byte)dovi_read_u_v(2, bits);
        dm[0x42] = (byte)dovi_read_u_v(2, bits);
        dm[0x43] = (byte)dovi_read_u_v(2, bits);
        write16(dm + 0x44, dovi_read_u_v(12, bits));
        write16(dm + 0x46, dovi_read_u_v(12, bits));
        write16(dm + 0x48, dovi_read_u_v(10, bits));
        dm[0x4a] = (byte)dovi_read_ue_v(bits);
        result = dovi_ext_blocks_read(d, dm + 0x4c, dm + 0x4a, 1);
        if (result < 0) {
            report_error(log, log_level, result, PARSER_DM_EXTENSION_LINE_BASE);
            return result;
        }
#if PARSER_DM_INITIAL_EXTENSION_ACCOUNTING
        initial_bits += (u32)result;
#endif
        if (more_extension_bytes(bits)) {
            count = (byte)dovi_read_ue_v(bits);
            result = dovi_ext_blocks_read(d, dm + 0x4c + (u32)dm[0x4a] * 0x108u, &count, 2);
            if (result < 0) {
                report_error(log, log_level, result, PARSER_DM_EXTENSION_LINE_BASE + 4u);
                return result;
            }
#if PARSER_DM_INITIAL_EXTENSION_ACCOUNTING
            initial_bits += (u32)result;
#endif
            if ((u32)count + dm[0x4a] < 0x100u) {
                dm[0x4a] = (byte)((u32)count + dm[0x4a]);
            } else {
                event_fn event;
                dm[0x4a] = 0xff;
                event = read_event(d + 0x316d8);
                if (event != 0)
                    event(
                        4, 0x106,
                        "Found more than 255 extension levels. Discard extension levels beyond 255.");
            }
        }
    } else {
        byte *blocks = d + 0x107c8;
        byte *scratch = d + 0x20ec0;
        count = (byte)dovi_read_ue_v(bits);
        result = dovi_ext_blocks_read(d, blocks, &count, 1);
        if (result < 0) {
            report_error(log, log_level, result, PARSER_DM_EXTENSION_LINE_BASE + 0x13u);
            return result;
        }
        /* ext_blocks_read returns zero on success; add its result to bits_read. */
        initial_bits += (u32)result;
        dovi_update_dm_extensions(dm, blocks, count, scratch);
        if (more_extension_bytes(bits)) {
            count = (byte)dovi_read_ue_v(bits);
            result = dovi_ext_blocks_read(d, blocks, &count, 2);
            if (result < 0) {
                report_error(log, log_level, result, PARSER_DM_EXTENSION_LINE_BASE + 0x1au);
                return result;
            }
#if PARSER_DM_SECOND_UPDATE_ACCOUNTING
            initial_bits += (u32)result;
#endif
            dovi_update_dm_extensions(dm, blocks, count, scratch);
        }
    }

    /* Reload the mode byte after callbacks. */
    if (header[0x14] == 0u) {
        store_md(d, dm);
    }
    return signed32(bits->bits_read - initial_bits);
}

#if PARSER_DECODER_EXTENSION_API
static
#endif
    void store_md(void *decoder, const void *dm)
{
    byte *d = (byte *)decoder;
    byte remove[256];
    zero_bytes(remove, 256);
    copy_bytes(d + 0x84, (const byte *)dm, DOVI_DM_PAYLOAD_SIZE);
    remove[1] = remove[2] = remove[3] = remove[4] = remove[5] = 1;
    remove[8] = remove[9] = 1;
    dovi_remove_dm_extensions(d + 0x84, remove, d + 0x20ec0);
}
