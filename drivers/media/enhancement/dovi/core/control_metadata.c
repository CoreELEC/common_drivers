/* Read, filter and prepare packed display metadata for HDMI output. */
#include "control_metadata.h"
#include "control_state.h"
#include "control_packets.h"
#include "trim_reduction.h"
#include "fixed_access.h"

typedef char control_unsigned_is_32[(sizeof(unsigned int) == 4) ? 1 : -1];
typedef char control_int_is_32[(sizeof(int) == 4) ? 1 : -1];

static unsigned int control_be32(const unsigned char *p)
{
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) |
           p[3];
}

static void control_copy(unsigned char *dst, const unsigned char *src, unsigned int count)
{
    unsigned int i;
    for (i = 0; i < count; ++i)
        dst[i] = src[i];
}

int read_md_with_logger(void *metadata, const char *input, int size, int *offset,
                        control_metadata_logger logger)
{
    unsigned char *md = (unsigned char *)metadata;
    const unsigned char *src = (const unsigned char *)input;
    unsigned int used = (unsigned int)size - (unsigned int)*offset;
    unsigned int i;
    if (used >= 71)
        used = 71;
    control_copy(md, src + *offset, used);
    *offset = fixed_signed32((unsigned int)*offset + used);
    if (used == 0)
        return 0;
    for (i = 0; i < md[70]; ++i) {
        unsigned char *record = md + 71 + i * 261;
        unsigned int length;
        int remaining = fixed_signed32((unsigned int)size - (unsigned int)*offset);
        if (remaining > 4) {
            control_copy(record, src + *offset, 5);
            *offset = fixed_signed32((unsigned int)*offset + 5);
            used += 5;
        }
        length = control_be32(record);
        if (length > 256) {
            if (logger)
                logger("ERR: Found ext block with invalid len %d[%d %d %d %d]\n", length);
            return -1;
        }
        remaining = fixed_signed32((unsigned int)size - (unsigned int)*offset);
        if ((int)length <= remaining) {
            control_copy(record + 5, src + *offset, length);
            *offset = fixed_signed32((unsigned int)*offset + length);
        }
    }
    md[62] = 1;
    return (int)used;
}

int read_md(void *metadata, const char *input, int size, int *offset)
{
    return read_md_with_logger(metadata, input, size, offset, 0);
}

void filter_md(void *current, const void *previous)
{
    unsigned char *md = (unsigned char *)current;
    const unsigned char *old = (const unsigned char *)previous;
    unsigned int i, j;
    for (i = 0; i < md[70]; ++i) {
        unsigned char *record = md + 71 + i * 261;
        const unsigned char *prior = old + 71 + i * 261;
        unsigned int next[3];
        if (record[4] != 1)
            continue;
        /* All inputs are read before any outputs: current==previous is valid. */
        for (j = 0; j < 3; ++j) {
            unsigned int at = 5 + j * 2;
            unsigned int c = ((unsigned int)record[at] << 8) | record[at + 1];
            unsigned int p = ((unsigned int)prior[at] << 8) | prior[at + 1];
            next[j] = p * 0xf333u + c * 0xccdu;
        }
        for (j = 0; j < 3; ++j) {
            record[5 + j * 2] = (unsigned char)(next[j] >> 24);
            record[6 + j * 2] = (unsigned char)(next[j] >> 16);
        }
    }
}

static int control_metadata_size(const unsigned char *md, unsigned int count)
{
    unsigned int i, size = 71;
    for (i = 0; i < count; ++i)
        size += 5 + control_be32(md + 71 + i * 261);
    return fixed_signed32(size);
}

static unsigned int control_sink_version(const unsigned char *ctx)
{
    const unsigned char *target =
        *(const unsigned char *const *)(ctx + CONTROL_CONTEXT_TARGET_PARAM_OFFSET);
    const unsigned char *p = target + CONTROL_TARGET_SINK_DM_VERSION_OFFSET;
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
}

void update_md_for_hdmi(void *context, void *metadata)
{
    unsigned char *ctx = (unsigned char *)context;
    unsigned char *md = (unsigned char *)metadata;
    unsigned int count, sink, read, write, kept;
    int size, reduce = 0, remove = 0;
    update_dm_l0_for_hdmi(context, metadata);
    update_dm_ext_for_hdmi(context, metadata);
    count = md[70];
    sink = control_sink_version(ctx);
    if (count == 0)
        return;
    size = control_metadata_size(md, count);
    if (sink < 3 && size > 512)
        reduce = 1;
    else if (size > 2048 || size > 509)
        reduce = 1;
    if (reduce) {
        (void)reduce_trims(ctx + CONTROL_CONTEXT_TARGET_PARAM_OFFSET, metadata);
        count = md[70];
        sink = control_sink_version(ctx);
        if (count == 0)
            return;
    }
    size = control_metadata_size(md, count);
    if (sink < 3 && size > 512)
        remove = 1;
    else if (size > 2048 || size > 509)
        remove = 1;
    if (!remove)
        return;
    kept = count;
    write = 0;
    for (read = 0; read < count; ++read) {
        unsigned char *record = md + 71 + read * 261;
        unsigned int level = record[4];
        if ((level == 7 || level >= 12) && level != 254 && level != 255) {
            kept = (kept - 1) & 255;
            continue;
        }
        control_copy(md + 71 + write * 261, record, 261);
        write = (write + 1) & 255;
    }
    md[70] = (unsigned char)kept;
    for (read = 0; read < kept; ++read) {
        unsigned char *record = md + 71 + read * 261;
        if (record[4] == 254)
            record[6] = 2;
    }
}

static void control_put16(unsigned char *dst, unsigned int value)
{
    dst[0] = (unsigned char)(value >> 8);
    dst[1] = (unsigned char)value;
}

void update_dm_l0_for_hdmi(void *context, void *metadata)
{
    unsigned char *ctx = (unsigned char *)context;
    unsigned char *md = (unsigned char *)metadata;
    unsigned char config[DM_CONFIG_BYTES];
    unsigned int color, range, i;
    unsigned short min_pq, max_pq;
    static const unsigned char matrices[4][18] = {
        {0x25,0x66,0,0,0x39,0x93,0x25,0x66,0xf9,0x27,0xee,0xe2,0x25,0x66,0x43,0xd9,0,0},
        {0x20,0,0,0,0x32,0x65,0x20,0,0xfa,1,0xf1,5,0x20,0,0x3b,0x61,0,0},
        {0x25,0x66,3,0xa6,7,0xad,0x25,0x66,0xfb,0xbe,4,0xfb,0x25,0x66,1,0x38,0xe6,0xb0},
        {0x20,0,3,0x1f,6,0x91,0x20,0,0xfc,0x5b,4,0x43,0x20,0,1,0xb,0xea,0x57}
    };
    static const unsigned char ipt_lms[18] = {
        0x42,0xb9,0xfe,0xa3,0xfe,0xa3,0xfe,0xa3,0x42,0xb9,
        0xfe,0xa3,0xfe,0xa3,0xfe,0xa3,0x42,0xb9
    };
    (void)dovi_GetDmCfg(config, *(void **)ctx);
    color = fixed_load_le32(config + DM_CONFIG_TGTSIGENV_OFFSET + DM_TARGET_SIGNAL_CLR_OFFSET);
    range = fixed_load_le32(config + DM_CONFIG_TGTSIGENV_OFFSET + DM_TARGET_SIGNAL_RNG_OFFSET);
    if (color == 0 || color == 3) {
        unsigned int choice = (color == 3 ? 2u : 0u) + (range != 0 ? 1u : 0u);
        control_copy(md + 2, matrices[choice], 18);
        for (i = 20; i < 32; ++i)
            md[i] = 0;
        md[20] = (unsigned char)(range == 0 ? 1 : 0);
        md[24] = 8;
        md[28] = 8;
        if (color == 3 && md[61] != 2)
            control_copy(md + 32, ipt_lms, 18);
    }
    md[50] = 255;
    md[51] = 255;
    for (i = 52; i < 60; ++i)
        md[i] = 0;
    (void)control_get_stb_tmm(&min_pq, 0, &max_pq, 0, *(void **)ctx);
    control_put16(md + 64, min_pq);
    control_put16(md + 66, max_pq);
    color = fixed_load_le32(config + DM_CONFIG_TGTSIGENV_OFFSET + DM_TARGET_SIGNAL_CLR_OFFSET);
    if (color == 0)
        md[61] = 0;
    else if (color == 3)
        md[61] = 2;
    else if (color == 1)
        md[61] = 1;
    md[62] = 1;
    md[63] = 1;
    md[60] = 12;
}

void update_dm_ext_for_hdmi(void *context, void *metadata)
{
    unsigned char *ctx = (unsigned char *)context;
    unsigned char *md = (unsigned char *)metadata;
    unsigned char config[DM_CONFIG_BYTES];
    unsigned int read = 0, write = 0, kept, level, j;
    (void)dovi_GetDmCfg(config, *(void **)ctx);
    if (fixed_load_le32(config + 8) == 0) {
        md[70] = 0;
        return;
    }
    kept = md[70];
    while (read < md[70]) {
        unsigned char *record = md + 71 + read * 261;
        unsigned char *dst = md + 71 + write * 261;
        level = record[4];
        if (level == 5 || level == 6) {
            kept = (kept - 1) & 255;
        } else if (level == 1) {
            unsigned short min_pq, max_pq;
            unsigned int middle, extended = 0;
            (void)dovi_GetStbCc(&min_pq, 0, &max_pq, 0, *(void **)ctx);
            middle = ((unsigned int)record[9] << 8) | record[10];
            for (j = 0; j < md[70]; ++j) {
                unsigned char *version = md + 71 + j * 261;
                if (version[4] == 254) {
                    extended = version[6] >= 2 && control_sink_version(ctx) >= 2;
                    break;
                }
            }
            if (!extended) {
                if (min_pq > 614)
                    min_pq = 614;
                if (max_pq < 2081)
                    max_pq = 2081;
                if (middle < (unsigned int)min_pq + 700)
                    middle = (unsigned int)min_pq + 700;
                if (middle > (unsigned int)max_pq - 1)
                    middle = (unsigned int)max_pq - 1;
            }
            /* Only PQ payload fields are written. Compaction deliberately
             * retains the old destination header and remaining payload. */
            control_put16(dst + 5, min_pq);
            control_put16(dst + 7, max_pq);
            control_put16(dst + 9, middle);
            write = (write + 1) & 255;
        } else {
            control_copy(dst, record, 261);
            if (level == 2) {
                const unsigned char *ext =
                    *(const unsigned char *const *)(ctx + CONTROL_CONTEXT_H_MDS_EXT_OFFSET);
                unsigned int area =
                    fixed_load_le32(ext + DM_EXTENSIONS_ACTIVE_AREA_RIGHT_OFFSET_OFFSET);
                unsigned int bottom =
                    (unsigned int)ext[DM_EXTENSIONS_ACTIVE_AREA_BOTTOM_OFFSET_OFFSET] |
                    ((unsigned int)ext[DM_EXTENSIONS_ACTIVE_AREA_BOTTOM_OFFSET_OFFSET + 1] << 8);
                if ((ext[DM_EXTENSIONS_LVL5AOIAVAIL_OFFSET] ||
                     ext[DM_EXTENSIONS_LVL5AOIAVAIL_OFFSET + 1]) &&
                    (area || bottom) && dst[9] > 7) {
                    dst[9] = 8;
                    dst[10] = 0;
                }
            }
            write = (write + 1) & 255;
        }
        read = (read + 1) & 255;
    }
    md[70] = (unsigned char)kept;
}
