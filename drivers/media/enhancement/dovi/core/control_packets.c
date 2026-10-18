/* Control metadata and HDR infoframe helpers. */
#include "fixed_access.h"
#include "control_packets.h"
#include "pq_conversion.h"
#include "control_abi_recovered.h"
#include "control_state.h"
#include "kernel_map_serialization.h"
#include "dm_layout.h"

typedef unsigned char byte;
typedef char require_u16[(sizeof(unsigned short) == 2) ? 1 : -1];
typedef char require_u32[(sizeof(unsigned int) == 4) ? 1 : -1];
typedef char require_u64[(sizeof(unsigned long long) == 8) ? 1 : -1];
typedef char require_ptr64[(sizeof(void *) == 8) ? 1 : -1];

static const unsigned char *read_ptr(const unsigned char *p)
{
    return (const unsigned char *)fixed_load_le64(p);
}

static void write_be16(unsigned char *p, unsigned int value)
{
    p[0] = (unsigned char)(value >> 8);
    p[1] = (unsigned char)value;
}

unsigned int dovi_get_dm_kernel_buf(const void *state, void *output)
{
    return dovi_DumpKsBinBuf(read_ptr((const unsigned char *)state + CONTROL_CONTEXT_H_KS_OFFSET),
                             output);
}

int dovi_get_output_format(const void *state, void *hdmi_format, void *core_format)
{
    DmCfgFxp_t config;
    unsigned char *hdmi_words = hdmi_format, *core_words = core_format;
    unsigned int mode;
    (void)dovi_GetDmCfg(&config,
                        read_ptr((const unsigned char *)state + CONTROL_CONTEXT_H_DM_OFFSET));
    fixed_store_le32(hdmi_words, (unsigned int)config.tgtSigEnv.Bdp);
    if (config.tgtSigEnv.Chrm == CChrm444 || config.tgtSigEnv.Chrm == CChrm422)
        fixed_store_le32(hdmi_words + 4, config.tgtSigEnv.Chrm);
    else if (config.tgtSigEnv.Chrm == CChrm420)
        fixed_store_le32(hdmi_words + 4, 0);
    if (config.tgtSigEnv.Clr == CClrYuv || config.tgtSigEnv.Clr == CClrRgb)
        fixed_store_le32(hdmi_words + 8, config.tgtSigEnv.Clr);
    else if (config.tgtSigEnv.Clr == CClrIpt)
        fixed_store_le32(hdmi_words + 8, 2);
    fixed_store_le32(hdmi_words + 12, 0);
    mode = fixed_load_le32((const unsigned char *)state +
                           CONTROL_CONTEXT_OUTPUT_OPERATING_MODE_OFFSET);
    if (mode == 0) {
        fixed_store_le32(core_words + 12, 0);
        fixed_store_le32(core_words + 4, 2);
        fixed_store_le32(core_words + 8, 2);
        fixed_store_le32(core_words, 12);
    } else if (mode == 1 || mode == 5) {
        fixed_store_le32(core_words + 4, 1);
        fixed_store_le32(core_words, 12);
        if (mode == 1) {
            fixed_store_le32(core_words + 12, 1);
            fixed_store_le32(core_words + 8, 2);
        } else {
            fixed_store_le32(core_words + 8, 0);
            fixed_store_le32(core_words + 12, 1);
        }
    } else if (mode < 5) {
        fixed_store_le32(core_words, mode == 4 ? 8 : 10);
        fixed_store_le32(core_words + 8, 0);
        fixed_store_le32(core_words + 4, 2);
        fixed_store_le32(core_words + 12, 0);
    }
    return 0;
}

int control_get_stb_tmm(unsigned short *min_pq, unsigned int *min, unsigned short *max_pq,
                        unsigned int *max, const void *dm)
{
    const unsigned char *h = (const unsigned char *)dm, *cfg;
    if (h == 0 || fixed_load_le32(h + 4) == 0)
        return -1;
    if (min_pq != 0)
        *min_pq = (unsigned short)fixed_load_le16(h + 0xa9c);
    if (min != 0) {
        cfg = read_ptr(h + 0x7b8);
        if (fixed_load_le32(cfg + 0x310) == 3u) {
            if (fixed_load_le32(cfg + 8) == 1u)
                *min = dovi_PQ12ToL((unsigned short)fixed_load_le16(h + 0xa9c));
            else
                *min = fixed_load_le32(cfg + 0x3b8);
        } else {
            *min = fixed_load_le32(read_ptr(h + 0x1128) + 0x2a4c);
        }
    }
    if (max_pq != 0)
        *max_pq = (unsigned short)fixed_load_le16(h + 0xa9e);
    if (max != 0) {
        cfg = read_ptr(h + 0x7b8);
        if (fixed_load_le32(cfg + 0x310) == 3u) {
            if (fixed_load_le32(cfg + 8) == 1u)
                *max = dovi_PQ12ToL((unsigned short)fixed_load_le16(h + 0xa9e));
            else
                *max = fixed_load_le32(cfg + 0x3bc);
        } else {
            *max = fixed_load_le32(read_ptr(h + 0x1128) + 0x2a50);
        }
    }
    return 0;
}

void get_fixed_metadata(const void *ctx, void *dm)
{
    static const byte fixed[64] = {
        0x00,0x01,0x20,0x00,0x03,0x1f,0x06,0x91,
        0x20,0x00,0xfc,0x5b,0x04,0x43,0x20,0x00,
        0x01,0x0b,0xea,0x57,0x00,0x00,0x00,0x00,
        0x08,0x00,0x00,0x00,0x08,0x00,0x00,0x00,
        0x42,0xb9,0xfe,0xa3,0xfe,0xa3,0xfe,0xa3,
        0x42,0xb9,0xfe,0xa3,0xfe,0xa3,0xfe,0xa3,
        0x42,0xb9,0xff,0xff,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x0c,0x02,0x01,0x01
    };
    unsigned char *p = (unsigned char *)dm;
    unsigned int i;
    unsigned short min_pq, max_pq;
    for (i = 0; i < 64; ++i)
        p[i] = fixed[i];
    (void)control_get_stb_tmm(&min_pq, 0, &max_pq, 0,
                              read_ptr((const unsigned char *)ctx + CONTROL_CONTEXT_H_DM_OFFSET));
    write_be16(p + 64, min_pq);
    write_be16(p + 66, max_pq);
    p[68] = 0;
    p[69] = 42;
    p[70] = 0;
}

int dovi_get_info_frame(const void *state, const void *target, int input_format, const void *hdr10,
                        const void *metadata, void *infoframe)
{
    static const byte primaries[16] = {
        0x48,0x8a,0x08,0x39,0x34,0x21,0xaa,0x9b,
        0x96,0x19,0xfc,0x08,0x13,0x3d,0x42,0x40
    };
    unsigned char *output = (unsigned char *)infoframe;
    const unsigned char *target_bytes = (const unsigned char *)target,
                        *hdr10_bytes = (const unsigned char *)hdr10;
    const unsigned char *metadata_bytes = (const unsigned char *)metadata, *extension;
    unsigned int i, minimum, maximum;
    fixed_store_le32(output, 0x021a0107);
    output[4] = 0;
    if (input_format == 1) {
        for (i = 0; i < 16; ++i)
            output[5 + i] = hdr10_bytes[8 + i];
        fixed_store_le16(output + 21, fixed_load_le32(hdr10_bytes + 4) / 10000u);
        fixed_store_le16(output + 23, fixed_load_le32(hdr10_bytes));
        for (i = 0; i < 4; ++i)
            output[25 + i] = hdr10_bytes[24 + i];
        return 0;
    }
    if (input_format != 0 && input_format != 2)
        return 0;
    for (i = 0; i < 16; ++i)
        output[5 + i] = primaries[i];
    if (input_format == 0 && fixed_load_le32((const unsigned char *)state +
                                             CONTROL_CONTEXT_DOVI2HDR10_NOMAPPING_OFFSET) != 0) {
        minimum = dovi_PQ12ToL(
            (unsigned short)(((unsigned int)metadata_bytes[64] << 8) + metadata_bytes[65]));
        maximum = dovi_PQ12ToL(
            (unsigned short)(((unsigned int)metadata_bytes[66] << 8) + metadata_bytes[67]));
        fixed_store_le16(output + 23, (unsigned int)(((unsigned long long)minimum * 10000u) >> 18));
        fixed_store_le16(output + 21, (unsigned int)(((unsigned long long)maximum * 10000u) >> 18));
        output[25] = output[26] = output[27] = output[28] = 0;
        for (i = 0; i < metadata_bytes[70]; ++i) {
            extension = metadata_bytes + CONTROL_METADATA_EXT_OFFSET + i * CONTROL_EXTENSION_BYTES;
            if (extension[4] == 6) {
                if (extension[5] != 0 || extension[6] != 0 || extension[7] != 0 ||
                    extension[8] != 0) {
                    output[21] = extension[6];
                    output[22] = extension[5];
                    output[23] = extension[8];
                    output[24] = extension[7];
                }
                output[25] = extension[10];
                output[26] = extension[9];
                output[27] = extension[12];
                output[28] = extension[11];
            }
        }
        return 0;
    }
    fixed_store_le16(output + 21,
                     fixed_load_le32(target_bytes + CONTROL_TARGET_TMAX_OFFSET) / 10000u);
    fixed_store_le16(output + 23, fixed_load_le32(target_bytes + CONTROL_TARGET_TMIN_OFFSET));
    output[25] = output[26] = output[27] = output[28] = 0;
    return 0;
}
