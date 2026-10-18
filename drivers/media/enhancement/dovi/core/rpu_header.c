/* Parse RPU header fields and update decoder sequence state. */
#include "parser_api.h"
#include "rpu_header.h"
#include "bitstream.h"

typedef unsigned char byte;
typedef unsigned int u32;
typedef char require_uint32[(sizeof(u32) == 4) ? 1 : -1];
typedef char require_int32[(sizeof(int) == 4) ? 1 : -1];
typedef char require_pointer64[(sizeof(void *) == 8 && sizeof(dovi_rpu_header_log_fn) == 8 &&
                                sizeof(dovi_rpu_header_event_fn) == 8)
                                   ? 1
                                   : -1];

static void copy_bytes(byte *dest, const byte *src, u32 count)
{
    while (count-- != 0)
        *dest++ = *src++;
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

static byte *read_pointer(const byte *p)
{
    byte *value;
    copy_bytes((byte *)&value, p, 8);
    return value;
}

static dovi_rpu_header_log_fn read_log(const byte *p)
{
    dovi_rpu_header_log_fn value;
    copy_bytes((byte *)&value, p, 8);
    return value;
}

static dovi_rpu_header_event_fn read_event(const byte *p)
{
    dovi_rpu_header_event_fn value;
    copy_bytes((byte *)&value, p, 8);
    return value;
}

static int signed32(u32 value)
{
    return (value & 0x80000000u) ? -1 - (int)~value : (int)value;
}

/* The diagnostic formats one unsigned-byte argument. */
#include "rpu_defaults_impl.inc"

static void read_pivots(byte *header, u32 count_offset, u32 array_offset, u32 depth_offset,
                        struct dovi_bitstream *bits)
{
    u32 i;
    for (i = 0; i < header[count_offset]; ++i) {
        u32 value = dovi_read_u_v(header[depth_offset], bits);
#if PARSER_FATAL_ARRAY_LIMITS
        /* Consume the seventeenth value before trapping. */
        if (i == 16u)
            parser_limit_trap();
#endif
        write16(header + array_offset + i * 2u, value);
    }
}

int dovi_rpu_header_read(void *ctx)
{
    byte *decoder = (byte *)ctx;
    byte *header = read_pointer(read_pointer(decoder));
    struct dovi_bitstream *bits = (struct dovi_bitstream *)(decoder + 0x40);
    u32 initial_bits = bits->bits_read;
    dovi_rpu_header_log_fn log = read_log(decoder + 0x315c8);
    u32 log_level = read32(decoder + 0x315d0);
    dovi_rpu_header_event_fn event;
    u32 value, component;

    header[0] = (byte)dovi_read_u_v(6, bits);
    if (header[0] < 2u) {
        if (log != 0 && log_level > 2u)
            log("RPU Error: RPU type: %d not supported\n", (int)header[0]);
        return -3;
    }
    write16(header + 2, dovi_read_u_v(11, bits));
    if (header[0] != 2u)
        return signed32(bits->bits_read - initial_bits);

    header[4] = (byte)dovi_read_u_v(4, bits);
    header[5] = (byte)dovi_read_u_v(4, bits);
    header[6] = (byte)parser_read_flag(bits);
    if (header[6] != 0) {
        header[8] = (byte)parser_read_flag(bits);
        header[9] = (byte)dovi_read_u_v(2, bits);
        if (header[9] != 0) {
            if (log != 0 && log_level > 2u)
                log("RPU Error: Only fixed coefficient data type supported.\n");
            return -3;
        }
        header[0x0a] = (byte)dovi_read_ue_v(bits);
        if (header[8] != 0)
            header[0x0c] = (byte)(dovi_read_ue_v(bits) + 6u);
        header[0x0d] = (byte)dovi_read_u_v(2, bits);
        header[0x0e] = (byte)parser_read_flag(bits);
        if ((header[3] & 7u) == 0) {
            header[0x0f] = (byte)(dovi_read_ue_v(bits) + 8u);
            header[0x10] = (byte)(dovi_read_ue_v(bits) + 8u);
            header[0x11] = (byte)(dovi_read_ue_v(bits) + 8u);
            header[0x12] = (byte)parser_read_flag(bits);
            if (header[0x12] != 0) {
                header[0x13] = (byte)parser_read_flag(bits);
                if (header[9] == 0 && header[0x13] != 0)
                    header[0x0b] = (byte)(dovi_read_ue_v(bits) + 6u);
            }
            header[0x14] = (byte)dovi_read_u_v(3, bits);
            header[0x15] = (byte)parser_read_flag(bits);
            header[0x16] = (byte)parser_read_flag(bits);
            if (header[0x16] == 0 && header[9] == 0 && header[8] != 0)
                header[0x17] = (byte)(dovi_read_ue_v(bits) + 10u);
        }
        copy_bytes(decoder + 0x64, header + 7, 17);
        write32(decoder + 0x78, 1);
    } else if (read32(decoder + 0x78) == 0) {
        event = read_event(decoder + 0x316d8);
        if (event != 0)
            event(4, 0x103, "No previous sequence header found, using default values.");
        default_sequence(header);
    } else {
        copy_bytes(header + 7, decoder + 0x64, 17);
    }

    header[0x18] = (byte)parser_read_flag(bits);
    header[0x19] = (byte)parser_read_flag(bits);
    if (header[0x19] != 0) {
        header[0x1a] = (byte)dovi_read_ue_v(bits);
        /* The mask uses the retained +0x1b, not the newly read +0x1a. */
        if (((read32(decoder + 0x7c) >> (header[0x1b] & 31u)) & 1u) == 0) {
            event = read_event(decoder + 0x316d8);
            if (event != 0) {
                char message[256];
                unavailable_id_message(message, header[0x1b]);
                event = read_event(decoder + 0x316d8);
                event(4, 0x104, message);
            }
            default_mapping(header);
        }
        return signed32(bits->bits_read - initial_bits);
    }

    header[0x1a] = 0;
    value = dovi_read_ue_v(bits);
    header[0x1b] = (byte)value;
    write32(decoder + 0x7c, read32(decoder + 0x7c) | (1u << (value & 31u)));
    header[0x1c] = (byte)dovi_read_ue_v(bits);
    header[0x1d] = (byte)dovi_read_ue_v(bits);
    if ((header[3] & 7u) == 0 && header[0x1d] != 0) {
        header[0x1e] = (byte)dovi_read_u_v(4, bits);
        if (header[0x1e] == 14u && header[0x1d] != 0) {
            if (header[0x1d] == 2u)
                header[0x1f] = (byte)(dovi_read_u_v(2, bits) * 2u + 4u);
            header[0x20] = (byte)(dovi_read_u_v(2, bits) * 2u + 4u);
        }
    }

    for (component = 0; component < 3u; ++component) {
        header[0x25 + component] = (byte)(dovi_read_ue_v(bits) + 2u);
        read_pivots(header, 0x25u + component, 0x28u + component * 0x20u, 0x0f, bits);
    }
    if ((header[3] & 7u) == 0 && header[0x12] != 0) {
        header[0xe8] = (byte)dovi_read_u_v(3, bits);
        if (header[0xe8] != 0) {
            header[0xe9] = (byte)dovi_read_u_v(3, bits);
            header[0xea] = (byte)dovi_read_u_v(3, bits);
            header[0xeb] = (byte)dovi_read_u_v(3, bits);
        }
        header[0xec] = (byte)dovi_read_u_v(3, bits);
        if (header[0xec] != 0) {
            header[0xed] = (byte)dovi_read_u_v(3, bits);
            header[0xee] = (byte)dovi_read_u_v(3, bits);
            header[0xef] = (byte)dovi_read_u_v(3, bits);
        }
    }
    if ((header[3] & 7u) == 0 && header[0x16] == 0) {
        header[0xf0] = (byte)dovi_read_u_v(3, bits);
        header[0xf1] = header[0x1d] == 2u ? (byte)(dovi_read_ue_v(bits) + 2u) : 2;
        read_pivots(header, 0xf1, 0xf2, 0x10, bits);
        if (header[0x1d] != 0)
            header[0x132] = (byte)dovi_read_u_v(3, bits);
    }
    header[0x133] = (byte)(dovi_read_ue_v(bits) + 1u);
    header[0x134] = (byte)(dovi_read_ue_v(bits) + 1u);
    if ((int)header[0x133] + (int)header[0x134] - 1 > 1 && header[0] == 2u)
        header[0x135] = (byte)dovi_read_ue_v(bits);
    return signed32(bits->bits_read - initial_bits);
}
