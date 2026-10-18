/* Control metadata and HDR infoframe helpers. */
#include "fixed_access.h"
#include "control_packets.h"
#include "control_memory.h"
#include "control_state.h"
#include "color_narrow.h"
#include "color_primaries.h"
#include "pq_conversion.h"
#include "crc32.h"
#include "dm_pipeline.h"

typedef char require_u16[(sizeof(unsigned short) == 2) ? 1 : -1];
typedef char require_u32[(sizeof(unsigned int) == 4) ? 1 : -1];
typedef char require_u64[(sizeof(unsigned long long) == 8) ? 1 : -1];
typedef char require_ptr64[(sizeof(void *) == 8) ? 1 : -1];

static void cpbytes(unsigned char *to, const unsigned char *from, unsigned long long n)
{
    unsigned long long i;
    for (i = 0; i < n; ++i)
        to[i] = from[i];
}

static void zero(unsigned char *to, unsigned int n)
{
    unsigned int i;
    for (i = 0; i < n; ++i)
        to[i] = 0;
}

static unsigned int be16(const unsigned char *p)
{
    return ((unsigned int)p[0] << 8) | p[1];
}

static unsigned int be32(const unsigned char *p)
{
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) |
           p[3];
}

static void limit_trap(void)
{
#if defined(_MSC_VER)
    __debugbreak();
#else
    __builtin_trap();
#endif
}

static const unsigned int output_format[4][6]={{12,12,10,10,8,12},{2,1,2,2,2,1},
                                    {2,2,0,0,0,0},{0,1,0,0,0,1}};
static const unsigned int source_defaults[5][9]={
    {1,0,1,2,1,1,1,0,1},
    {262144000,78643200,262144000,262144000,262144000,262144000,262144000,78643200,262144000},
    {0,39322,0,0,0,0,0,39322,0},{3,2,3,3,3,3,3,2,3},{2,1,2,2,2,2,2,1,2}};
static const unsigned int signal_code[8]={3,5,1,7,1,1,1,5};

unsigned int dovi_GetByteField(unsigned int value, unsigned int mask)
{
    unsigned int shift = 0, bits = mask & 255;
    /* The original never terminates for an all-zero low-byte mask. */
    while ((bits & 1) == 0) {
        ++shift;
        bits >>= 1;
    }
    return (value & mask & 255) >> ((shift & 255) & 31);
}

unsigned int dovi_get_data_set_length(const void *packet)
{
    return be16((const unsigned char *)packet + 8);
}

int dovi_vsem_unpack(const void *packets, void *payload)
{
    const unsigned char *in = (const unsigned char *)packets, *next;
    unsigned char *out = (unsigned char *)payload, *cursor;
    unsigned int length, consumed;
    if (!(in[1] & 128))
        return -1;
    length = be16(in + 8);
    cpbytes(out, in + 3, 28);
    next = in + 34;
    cursor = out;
    consumed = 21;
    for (;;) {
        unsigned char *dest = cursor + 28;
        if (next[-2] & 64) {
            cpbytes(dest, next, (unsigned int)(length - consumed));
            break;
        }
        cpbytes(dest, next, 28);
        if (length <= consumed + 28) {
            dovi_control_os.printk("didn't find last packet!\n");
            break;
        }
        next += 31;
        cursor = dest;
        if (consumed >= 0xfc8) {
            dovi_control_os.printk("vsem payload buffer is too small!\n");
            break;
        }
        consumed += 28;
    }
    if (length != 6 && dovi_rpu_crc32(0, out + 13, (unsigned int)(length - 6)) == 0)
        return 0;
    dovi_control_os.printk("Error: crc error while reading control data\n");
    return -1;
}

int dovi_get_info_frame(const void *state, const void *target, const void *metadata,
                        void *infoframe)
{
    const unsigned char *state_bytes = (const unsigned char *)state,
                        *target_bytes = (const unsigned char *)target,
                        *metadata_bytes = (const unsigned char *)metadata;
    unsigned char *output = (unsigned char *)infoframe;
    unsigned int i;
    unsigned long long value;
    fixed_store_le32(output, 0x021a0107);
    output[4] = 0;
    for (i = 0; i < 8; ++i) {
        value = (unsigned long long)((long long)(int)fixed_load_le32(target_bytes + 0x54 + i * 4) *
                                         50000 +
                                     0x2000000);
        output[5 + i * 2] = (unsigned char)(value >> 26);
        output[6 + i * 2] = (unsigned char)(value >> 34);
    }
    if (!metadata_bytes || !fixed_load_le32(state_bytes + 0x144c)) {
        value = fixed_load_le32(target_bytes + 0x14);
        output[21] = (unsigned char)(value >> 18);
        output[22] = (unsigned char)(value >> 26);
        value = (unsigned long long)fixed_load_le32(target_bytes + 0x10) * 10000;
        output[23] = (unsigned char)(value >> 18);
        output[24] = (unsigned char)(value >> 26);
        fixed_store_le32(output + 25, 0);
    } else {
        const unsigned char *last_extension = 0, *extension = metadata_bytes + 0x47;
        unsigned long long minimum =
            (unsigned long long)dovi_PQ12ToL(be16(metadata_bytes + 0x40)) * 10000;
        value = dovi_PQ12ToL(be16(metadata_bytes + 0x42));
        output[23] = (unsigned char)(minimum >> 18);
        output[24] = (unsigned char)(minimum >> 26);
        fixed_store_le32(output + 25, 0);
        output[21] = (unsigned char)(value >> 18);
        output[22] = (unsigned char)((value >> 26) & 63);
        for (i = 0; i < metadata_bytes[0x46]; ++i) {
            if (extension[4] == 6)
                last_extension = extension + 5;
            extension += 5 + be32(extension);
        }
        if (last_extension) {
            if (last_extension[0] || last_extension[1] || last_extension[2] || last_extension[3]) {
                output[21] = last_extension[1];
                output[22] = last_extension[0];
                output[23] = last_extension[3];
                output[24] = last_extension[2];
            }
            output[25] = last_extension[5];
            output[26] = last_extension[4];
            output[27] = last_extension[7];
            output[28] = last_extension[6];
        }
    }
    if (fixed_load_le32(state_bytes + 0x1438))
        fixed_store_le32(output + 25, 0);
    return 0;
}

int dovi_get_output_format(const void *state, void *hdmi_format, void *core_format)
{
    const unsigned char *state_bytes = (const unsigned char *)state,
                        *context = (const unsigned char *)fixed_load_le64(state_bytes);
    const unsigned char *format = context ? context + 8 : 0;
    unsigned char *hdmi_words = hdmi_format, *core_words = core_format;
    unsigned int i, index;
    if (hdmi_words) {
        cpbytes(hdmi_words + 0x40, state_bytes + 0x1494, 12);
        fixed_store_le32(hdmi_words, fixed_load_le32(format + 0x8c));
        index = fixed_load_le32(format + 0x80);
        if (index < 3)
            fixed_store_le32(hdmi_words + 4, index);
        index = fixed_load_le32(format + 0x7c);
        if (index == 0 || index == 1 || index == 3)
            fixed_store_le32(hdmi_words + 8, index == 3 ? 2 : index);
        fixed_store_le32(hdmi_words + 24, fixed_load_le32(state_bytes + 0x1480) == 0);
        fixed_store_le32(hdmi_words + 28, fixed_load_le32(state_bytes + 0x143c));
        dovi_GetStbTmmIdx(1, hdmi_words + 20, hdmi_words + 12, hdmi_words + 22, hdmi_words + 16,
                          context);
        if (fixed_load_le32(state_bytes + 0x143c))
            for (i = 0; i < 8; ++i)
                fixed_store_le32(hdmi_words + 32 + i * 4,
                                 fixed_load_le32(state_bytes + 0x14a0 + i * 4));
    }
    if (core_words) {
        index = fixed_load_le32(state_bytes + 0x1450);
        if (index < 6) {
            fixed_store_le32(core_words, output_format[0][index]);
            fixed_store_le32(core_words + 4, output_format[1][index]);
            fixed_store_le32(core_words + 8, output_format[2][index]);
            fixed_store_le32(core_words + 24, output_format[3][index]);
        }
        dovi_GetStbTmmIdx(1, core_words + 20, core_words + 12, core_words + 22, core_words + 16,
                          context);
        if (fixed_load_le32(state_bytes + 0x143c))
            for (i = 0; i < 8; ++i)
                fixed_store_le32(core_words + 32 + i * 4,
                                 fixed_load_le32(state_bytes + 0x14a0 + i * 4));
    }
    return 0;
}

int dovi_update_src_param(void *source, int format)
{
    unsigned char *s = (unsigned char *)source, matrix[18] = {0};
    unsigned int index = (unsigned int)format - 1, scale;
    if (fixed_load_le32(s + 0x74) != 1)
        return 0;
    dovi_GetYuvRgbOffFxp(1, fixed_load_le32(s + 0x58), s + 0x8c);
    if (index >= 9 || !((0x18bu >> index) & 1))
        return -1;
    scale = dovi_GetYuv2RgbM33Fxp(source_defaults[3][index], matrix);
    fixed_store_le16(s + 0x98, scale);
    cpbytes(s + 0x78, matrix, 18);
    return 0;
}

int dovi_set_default_src_param(void *source, int format)
{
    unsigned char *s = (unsigned char *)source, matrix[18] = {0};
    unsigned int depth, index = (unsigned int)format - 1, scale;
    fixed_store_le32(s + 8, 0x180000);
    zero(s + 0x38, 32);
    depth = (format == 2 || format == 9) ? 8 : 10;
    if (fixed_load_le32(s + 12) != 2)
        depth = 12;
    zero(s + 0x5c, 8);
    fixed_store_le32(s + 0x74, 0);
    fixed_store_le32(s + 0x9c, 0);
    fixed_store_le32(s + 0x58, depth);
    dovi_GetYuvRgbOffFxp(0, depth, s + 0x8c);
    if (index >= 9 || !((0x18bu >> index) & 1))
        return -1;
    fixed_store_le32(s + 100, 0x51e);
    fixed_store_le32(s + 0x68, source_defaults[1][index]);
    fixed_store_le32(s + 0x6c, source_defaults[0][index]);
    fixed_store_le32(s + 0x70, source_defaults[2][index]);
    scale = dovi_GetYuv2RgbM33NarrowFxp(source_defaults[3][index], depth, matrix);
    fixed_store_le16(s + 0x98, scale);
    cpbytes(s + 0x78, matrix, 18);
    dovi_GetPrimaries(source_defaults[4][index], s + 0x14);
    return 0;
}

int dovi_set_default_target_param(void *target, int format)
{
    unsigned char *t = (unsigned char *)target, matrix[18] = {0};
    unsigned int black, depth, kind, matrix_id, primaries, peak, scale;
    fixed_store_le32(t + 0x2c, 0);
    fixed_store_le64(t + 0x24, 1);
    dovi_GetYuvRgbOffFxp(0, 12, t + 0x44);
    if (format == 1 || format == 4) {
        black = 0;
        depth = 10;
        kind = format == 1 ? 1 : 2;
        primaries = 2;
        matrix_id = 3;
        peak = 0xfa00000;
    } else if (format == 2 || format == 8) {
        kind = 0;
        depth = format == 2 ? 8 : 10;
        primaries = 1;
        matrix_id = 2;
        black = 0x999a;
        peak = 0x1900000;
    } else
        return -1;
    fixed_store_le32(t + 0x1c, black);
    fixed_store_le32(t + 0x20, depth);
    fixed_store_le32(t + 0x18, kind);
    fixed_store_le32(t + 0x10, 0x51e);
    fixed_store_le32(t + 0x14, peak);
    scale = dovi_GetRgb2YuvM33NarrowFxp(matrix_id, 12, matrix);
    fixed_store_le16(t + 0x50, scale);
    cpbytes(t + 0x30, matrix, 18);
    dovi_GetPrimaries(primaries, t + 0x54);
    return 0;
}

void dovi_parse_vsif_from_PB4(const void *packet, void *control)
{
    const unsigned char *p = (const unsigned char *)packet;
    unsigned char *c = (unsigned char *)control;
    fixed_store_le32(c, p[0] & 1);
    fixed_store_le32(c + 16, (p[0] >> 1) & 15);
    fixed_store_le32(c + 4, p[1] >> 7);
    fixed_store_le32(c + 20, (p[1] >> 6) & 1);
    fixed_store_le32(c + 24, (p[1] >> 5) & 1);
    fixed_store_le32(c + 36, (p[1] >> 4) & 1);
    fixed_store_le32(c + 12, (p[1] & 128) ? (p[2] | ((unsigned int)(p[1] & 15) << 8)) : 0);
    if (p[1] & 64)
        cpbytes(c + 28, p + 3, 3);
    else
        zero(c + 28, 3);
    if (p[1] & 32)
        cpbytes(c + 31, p + 6, 4);
    else
        zero(c + 31, 4);
}

long long dovi_get_dolby_control_data(void *state, void *packet, void *is_standard, void *control)
{
    unsigned char *s = (unsigned char *)state, *p = (unsigned char *)packet,
                  *c = (unsigned char *)control;
    const unsigned char *block, *last = 0;
    unsigned int index, i, count;
    int result;
    unsigned char sum = 0;
    fixed_store_le32((unsigned char *)is_standard, fixed_load_le32(s + 0x1484) == 3);
    fixed_store_le64(c + 4, 0);
    fixed_store_le32(c + 12, 0);
    fixed_store_le32(c, fixed_load_le32(s + 0x143c));
    fixed_store_le32(c + 16, fixed_load_le32(s + 0xd24) == 0);
    index = fixed_load_le32(s + 0xd24) - 1;
    fixed_store_le32(c + 16, index < 8 ? signal_code[index] : 1);
    fixed_store_le32(c + 20, fixed_load_le32(s + 0x1464));
    if (fixed_load_le32(c + 20))
        cpbytes(c + 28, s + 0x1468, 3);
    else
        zero(c + 28, 3);
    result = dovi_dm_GetMdsOutput(s + 0x14c0, 0x800, 0, 0, 1, (void *)fixed_load_le64(s));
    if (result > 0) {
        block = s + 0x1507;
        count = s[0x1506];
        for (i = 0; i < count; ++i) {
            if (block[4] == 11)
                last = block + 5;
            block += 5 + be32(block);
        }
    }
    fixed_store_le32(c + 24, last && fixed_load_le32(s + 0x7c));
    if (last && fixed_load_le32(s + 0x7c))
        cpbytes(c + 31, last, 4);
    else
        zero(c + 31, 4);
    if (!p)
        return 0xffffffffll;
    p[0] = 0x81;
    p[1] = 1;
    if (fixed_load_le32(s + 0x147c) == 2 ||
        (fixed_load_le32(s + 0x147c) == 1 && fixed_load_le32(s + 0x1478) == 12)) {
        zero(p + 2, 29);
        p[2] = 27;
        p[4] = 0x46;
        p[5] = 0xd0;
        p[7] = (unsigned char)((fixed_load_le32(c) & 1) | ((fixed_load_le32(c + 16) & 15) << 1));
        p[8] = (unsigned char)((c[4] << 7) | ((c[20] & 1) << 6) | ((c[24] & 1) << 5));
        if (fixed_load_le32(c + 4)) {
            p[8] |= c[13] & 15;
            p[9] = (unsigned char)fixed_load_le32(c + 12);
        }
        if (fixed_load_le32(c + 20))
            cpbytes(p + 10, c + 28, 3);
        if (fixed_load_le32(c + 24))
            cpbytes(p + 13, c + 31, 4);
        count = 31;
    } else {
        zero(p + 2, 26);
        p[2] = 24;
        p[4] = 3;
        p[5] = 12;
        count = 28;
    }
    for (i = 0; i < count; ++i)
        sum = (unsigned char)(sum + p[i]);
    p[3] = (unsigned char)(0 - sum);
    return count;
}

void dovi_dmKs2dmreg_core3(const void *state, const void *kernel, void *registers,
                           const void *metadata, unsigned int length)
{
    const unsigned char *s = (const unsigned char *)state, *k = (const unsigned char *)kernel,
                        *m = (const unsigned char *)metadata;
    const unsigned char *data = (const unsigned char *)fixed_load_le64(k + 0x15f8);
    unsigned char *o = (unsigned char *)registers;
    unsigned int i;
    static const unsigned int from[]={2,6,10,14,22,26,30,34,40,44,96,100,104,108,52,64,68,72,76,84,88,92};
    static const unsigned int to[]={24,28,32,36,44,48,52,56,64,68,72,76,80,84,88,96,100,104,108,116,120,124};
    fixed_store_le32(o + 4, fixed_load_le32(s + 0x1450));
    for (i = 0; i < 22; ++i)
        fixed_store_le32(o + to[i], fixed_load_le32(data + from[i]));
    fixed_store_le32(o + 0x28, (fixed_load_le16(data) << 16) | fixed_load_le16(data + 18));
    fixed_store_le32(o + 0x3c, (fixed_load_le16(data + 20) << 16) | fixed_load_le16(data + 38));
    fixed_store_le32(o + 0x5c, fixed_load_le16(data + 48));
    fixed_store_le32(o + 0x70, (fixed_load_le16(data + 62) << 16) | fixed_load_le16(data + 80));
    fixed_store_le32(o + 0x3e0, fixed_load_le32(s + 0x1454) |
                                    ((fixed_load_le32(s + 0x145c) & 1) << 5) |
                                    ((fixed_load_le32(s + 0x1458) & 15) << 1));
    for (i = 1; i <= 0x34b; ++i)
        o[0x93 + i] = (i < length) ? m[i] : 0;
    fixed_store_le32(o + 0x90, m ? ((unsigned int)m[0] | (length << 8)) : 0);
}

unsigned int dovi_get_dm_kernel_buf(const void *state, void *output, unsigned int index)
{
    if (index >= 4) {
        limit_trap();
        return 0;
    }
    return dovi_DumpKsBinBuf((const void *)fixed_load_le64((const unsigned char *)state + 0x28 +
                                                           (unsigned long long)index * 8),
                             output);
}

int dovi_get_hdmi_metadata_buffer(const void *state, void *output)
{
    return dovi_dm_GetMdsOutput(output, 0x1fd, 0, 0, 1,
                                (void *)fixed_load_le64((const unsigned char *)state));
}

void dovi_cb_content_info(unsigned char content, unsigned char white, unsigned char extra1,
                          unsigned char extra2)
{
    if (dovi_control26_debug & 2) {
        dovi_control_os.printk("Content type %d\n", content);
        dovi_control_os.printk("Desired White Point %d\n", white);
        dovi_control_os.printk("L11_byte2 %d\n", extra1);
        dovi_control_os.printk("L11_byte3 %d\n", extra2);
    }
    dovi_control26_content_type = content;
    dovi_control26_white_point = white;
    dovi_control26_l11_byte2 = extra1;
    dovi_control26_l11_byte3 = extra2;
}
