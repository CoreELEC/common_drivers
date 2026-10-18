/* Convert RPU mapping coefficients into a sparse composer configuration. */
#include "parser_api.h"
#include "composer_conversion.h"
typedef unsigned char byte;
typedef unsigned int u32;
typedef char require_uint32[(sizeof(u32) == 4) ? 1 : -1];

static u32 read16(const byte *p)
{
    return (u32)p[0] | ((u32)p[1] << 8);
}

static u32 read32(const byte *p)
{
    return read16(p) | (read16(p + 2) << 16);
}

static void write32(byte *p, u32 value)
{
    p[0] = (byte)value;
    p[1] = (byte)(value >> 8);
    p[2] = (byte)(value >> 16);
    p[3] = (byte)(value >> 24);
}

static void copy32(byte *dest, const byte *source)
{
    write32(dest, read32(source));
}

void dovi_comp_rpu_2_main_cfg(const void *header, const void *payload, void *output)
{
    const byte *h = (const byte *)header;
    const byte *p = (const byte *)payload;
    byte *out = (byte *)output;
    u32 channel, i;
    write32(out + 4, h[0x0f]);
    write32(out + 8, h[0x10]);
    write32(out, h[0x11]);
    write32(out + 0x0c, h[0x0a]);
    out[0x49c] = h[0xf0];
    out[0x49d] = h[0x16];
    out[0x49e] = h[0x15];
    out[0x49f] = h[0x0e];

    for (channel = 0; channel < 3u; ++channel) {
        u32 count = h[0x25 + channel];
        u32 mode = p[channel * 15u];
        const byte *nlq;
        write32(out + 0x10 + channel * 4u, count);
        write32(out + 0x88 + channel * 4u, mode);
        for (i = 0; i < count && (!PARSER_FATAL_ARRAY_LIMITS || i < 9u); ++i)
            write32(out + 0x1c + channel * 0x24u + i * 4u,
                    read16(h + 0x88 + channel * 0x20u + i * 2u));
#if PARSER_FATAL_ARRAY_LIMITS
        if (count > 9u)
            parser_limit_trap();
#endif

        if (mode == 0) {
            u32 segment;
            if (count < 2u) {
                count = 2;
                write32(out + 0x10 + channel * 4u, count);
            }
            for (segment = 0; segment < count - 1u; ++segment) {
                const byte *record = p + 0x5c + channel * 0x30cu + segment * 0x34u;
                u32 order;
#if PARSER_FATAL_ARRAY_LIMITS
                if (segment == 8u)
                    parser_limit_trap();
#endif
                order = record[0];
                write32(out + 0x94 + channel * 0x20u + segment * 4u, order);
                for (i = 0; i <= order && (!PARSER_FATAL_ARRAY_LIMITS || i < 3u); ++i) {
                    copy32(out + 0xf4 + channel * 0x60u + segment * 12u + i * 4u,
                           record + 0x0c + i * 4u);
                    copy32(out + 0x214 + channel * 0x60u + segment * 12u + i * 4u,
                           record + 0x20 + i * 4u);
                }
#if PARSER_FATAL_ARRAY_LIMITS
                if (order > 2u)
                    parser_limit_trap();
#endif
            }
        } else if (mode == 1u && channel != 0) {
            const byte *record = p + 0x5c + channel * 0xa8cu;
            u32 row = channel - 1u, order = record[0];
            write32(out + 0x334 + row * 4u, order);
            copy32(out + 0x33c + row * 0x58u, record + 4);
            copy32(out + 0x3ec + row * 0x58u, record + 8);
            for (i = 0; i < order * 7u && (!PARSER_FATAL_ARRAY_LIMITS || i < 21u); ++i) {
                copy32(out + 0x340 + row * 0x58u + i * 4u, record + 0x0c + i * 4u);
                copy32(out + 0x3f0 + row * 0x58u + i * 4u, record + 0x60 + i * 4u);
            }
#if PARSER_FATAL_ARRAY_LIMITS
            if (order > 3u)
                parser_limit_trap();
#endif
        }
        nlq = p + 0x6874 + channel * 0x1cu;
        write32(out + 0x4a0 + channel * 4u, read16(nlq));
        copy32(out + 0x4ac + channel * 12u, nlq + 0x0c);
        copy32(out + 0x4d0 + channel * 12u, nlq + 0x10);
        copy32(out + 0x4b0 + channel * 12u, nlq + 0x04);
        copy32(out + 0x4d4 + channel * 12u, nlq + 0x08);
        copy32(out + 0x4b4 + channel * 12u, nlq + 0x14);
        copy32(out + 0x4d8 + channel * 12u, nlq + 0x18);
    }
    if (h[0x12] != 0) {
        write32(out + 0x4f4, h[0x12]);
        write32(out + 0x4f8, h[0x13]);
        write32(out + 0x500, h[0xe8]);
        write32(out + 0x504, h[0xec]);
        for (i = 0; i < 3u; ++i) {
            write32(out + 0x508 + i * 4u, h[0xe9 + i]);
            write32(out + 0x514 + i * 4u, h[0xed + i]);
        }
        if (h[0x13] != 0) {
            u32 group;
            write32(out + 0x4fc, h[0x0b]);
            copy32(out + 0x520, p + 0x653c);
            copy32(out + 0x524, p + 0x6540);
            for (group = 0; group < 3u; ++group) {
                const byte *filter = p + 0x6544 + group * 0x20u;
                for (i = 0; i < 8u; ++i) {
                    copy32(out + 0x528 + group * 0x20u + i * 4u, filter + i * 4u);
                    copy32(out + 0x588 + group * 0x20u + i * 4u, filter + 0x180 + i * 4u);
                }
                for (i = 0; i < 6u; ++i) {
                    copy32(out + 0x5e8 + group * 0x18u + i * 4u, filter + 0x0c0 + i * 4u);
                    copy32(out + 0x678 + group * 0x18u + i * 4u, filter + 0x240 + i * 4u);
                    copy32(out + 0x630 + group * 0x18u + i * 4u, filter + 0x120 + i * 4u);
                    copy32(out + 0x6c0 + group * 0x18u + i * 4u, filter + 0x2a0 + i * 4u);
                }
            }
        }
    }
}
