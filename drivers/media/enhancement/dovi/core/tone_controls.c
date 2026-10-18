/* Prepare source and target fields for tone-curve generation. */
#include "fixed_access.h"
#include "tone_controls.h"
#include "tone_curve.h"
#include "fixed_arithmetic.h"
#include "dm_trim.h"
#include "fixed_transcendentals.h"
#include "pq_conversion.h"

typedef char tone_require_int[(sizeof(int) == 4) ? 1 : -1];
typedef char tone_require_short[(sizeof(short) == 2) ? 1 : -1];
typedef char tone_require_source[(sizeof(struct tone_curve_source) == 56) ? 1 : -1];
typedef char tone_require_display[(sizeof(struct tone_curve_display) == 48) ? 1 : -1];
typedef char tone_require_pair[(sizeof(struct tone_curve_pair) == 8) ? 1 : -1];

static unsigned int rd32(const void *p, unsigned int at)
{
    return fixed_load_le32((const unsigned char *)p + at);
}

static unsigned short rd16(const void *p, unsigned int at)
{
    return (unsigned short)fixed_load_le16((const unsigned char *)p + at);
}

static void wr32(void *p, unsigned int at, unsigned int x)
{
    fixed_store_le32((unsigned char *)p + at, x);
}

static void wr16(void *p, unsigned int at, unsigned int x)
{
    fixed_store_le16((unsigned char *)p + at, x);
}

void update_tc_ctrl_level(const void *tm, void *tc)
{
    unsigned int i;
    for (i = 1; i < 7; ++i)
        wr32(tc, 116 + i * 4, rd32(tm, 48 + i * 4) + rd32(tm, 76 + i * 4));
    if (rd32(tc, 56) == 1 && rd32(tc, 60) == 0)
        wr32(tc, 136, rd32(tm, 68) + rd32(tm, 132));
    dovi_ClampLevel2((int *)((unsigned char *)tc + 116));
    wr32(tc, 140, (rd32(tc, 140) - 2048u) * 2u);
    for (i = 1; i < 4; ++i)
        wr32(tc, 144 + i * 4, rd32(tm, 136 + i * 4));
}

void tm_ctrl_to_tc_ctrl(const void *tm, const void *dm, void *tc, int graphics_source)
{
    unsigned int direction = rd32(tc, 56), invertible = 0, rolloff, i;
    if (rd32(tm, 0) != 0 && direction == 1 && graphics_source != 0 && rd32(dm, 0) == 1)
        invertible = 1;
    wr32(tc, 60, invertible);
    wr32(tc, 64, rd32(tm, 4));
    rolloff = rd32(tm, 4);
    wr32(tc, 68, rolloff ? 0x80000000u / rolloff : 0x80000000u);
    wr32(tc, 72, rd32(tm, 8));
    wr32(tc, 76, rd32(tm, 12));
    wr32(tc, 80, rd32(tm, 20));
    wr32(tc, 84, rd32(tm, 28));
    if (direction == 1 && invertible == 0) {
        wr32(tc, 76, rd32(tm, 16));
        wr32(tc, 80, rd32(tm, 24));
    }
    for (i = 0; i < 5; ++i)
        wr16(tc, 104 + i * 2, rd16(tm, 32 + i * 2));
    if (direction == 1 && invertible == 0)
        wr16(tc, 108, 0u - rd16(tm, 36));
}

void update_tc_signal_hlg_a(int minimum, int maximum, void *signal)
{
    int input = fixed_signed32((unsigned int)maximum << 9);
    int l = dovi_log10Taylor(input / 1000);
    wr32(signal, 12, (unsigned int)minimum);
    wr32(signal, 16, (unsigned int)maximum - (unsigned int)minimum);
    wr32(signal, 8, (unsigned int)l * 0x1ae1u + 0x4cccu);
}

void target_signal_to_tc_ctrl(const void *target, void *tc)
{
    unsigned int pair = rd32(target, 192);
    unsigned char *signal = tc + 28;
    if (!pair) {
        unsigned int maximum = rd32(target, 188), diagonal = rd16(target, 196);
        wr16(signal, 0, dovi_LToPQ12(rd32(target, 184)));
        unsigned short max_pq = dovi_LToPQ12(maximum);
        unsigned int hlg;
        wr16(signal, 4, diagonal);
        wr16(signal, 2, max_pq);
        hlg = rd32(target, 40) == 3 ? 1u : 0u;
        wr16(signal, 6, hlg);
        if (hlg)
            update_tc_signal_hlg_a(fixed_signed32(rd32(target, 184)),
                                   fixed_signed32(rd32(target, 188)), signal);
    } else {
        wr16(signal, 0, rd16(target, 192));
        wr16(signal, 2, rd16(target, 194));
        wr16(signal, 4, rd16(target, 196));
    }
}
