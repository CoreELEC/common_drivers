/* Persistent control-state initialization and access. */
#include "control_state.h"
#include "control_packets.h"
#include "fixed_access.h"
#include "control_memory.h"
#include "color_basis.h"
#include "dm_pipeline.h"

typedef char require_widths[(sizeof(unsigned int) == 4 && sizeof(unsigned long long) == 8 &&
                             sizeof(void *) == 8)
                                ? 1
                                : -1];
#if defined(_MSC_VER)
void __debugbreak(void);
#pragma intrinsic(__debugbreak)

__declspec(noreturn) static void limit_trap(void)
{
    __debugbreak();
    __assume(0);
}
#else
__attribute__((noreturn)) static void limit_trap(void)
{
#if defined(__aarch64__)
    __asm__ volatile("brk #0x5512");
    __builtin_unreachable();
#else
    __builtin_trap();
#endif
}
#endif

dovi_content_info_callback dovi_content_info_cb_handle;
typedef char content_callback_pointer64[(sizeof(dovi_content_info_callback) == 8) ? 1 : -1];

void *dovi_InitDm(int count, const void *key_slots, void *control)
{
    unsigned char *c = (unsigned char *)control;
    const unsigned char *slots = (const unsigned char *)key_slots;
    unsigned int i;
    fixed_store_le32(c + 4, 0);
    if (count >= 0) {
        static const unsigned int offsets[5]={0x908,0xbd0,0xe98,0x1160,0x1428};
        for (i = 0; i < 5u; ++i) {
            fixed_store_le32(c + offsets[i], 0);
            if ((unsigned int)count == i)
                break;
        }
        if (count > 4)
            limit_trap();
    }
    fixed_store_le32(c + 0x16e8, 0);
    c[0x16ec] = 0;
    fixed_store_le64(c + 0x36f0, (unsigned long long)(c + 0x26ed));
    fixed_store_le32(c + 0x4398, 0);
    dovi_control_os.memset(c + 0x43a0, 0, 0x9ea0);
    fixed_store_le32(c + 0x43d8, 0x51f);
    fixed_store_le32(c + 0x43f0, fixed_sign16(dovi_GetLms2IctcpDmM33(c + 0x43dc)));
    fixed_store_le32(c + 0x4408, fixed_sign16(dovi_GetIctcpDm2LmsM33(c + 0x43f4)));
    fixed_store_le64(c + 0x4420, 0x800000008000ull);
    fixed_store_le64(c + 0x4428, 0xf00008000ull);
    for (i = 0; count > 0 && i < (unsigned int)count; ++i) {
        unsigned char *run = c + 0x101b8 + (unsigned long long)i * 0x1f78u;
        unsigned char *selected;
        fixed_store_le64(c + 0x4448 + i * 0x28u, 0x800000008000ull);
        fixed_store_le64(c + 0x4450 + i * 0x28u, 0xf00008000ull);
        if (fixed_load_le32(c + 0x43b8) != fixed_load_le32(c + 0x43bc) && i == 0)
            selected = c + 0xe240;
        else {
            selected = run;
            if (i + 1u > 5u)
                limit_trap();
        }
        fixed_store_le32(selected + 0xd8, 0xfffffffeu);
        selected = run;
        if (i == 0 && fixed_load_le32(c + 0x43b8) != fixed_load_le32(c + 0x43bc))
            selected = c + 0xe240;
        fixed_store_le32(selected + 0x68, 0xffffffffu);
        fixed_store_le64(c + 0x1f918 + i * 8u, fixed_load_le64(slots + i * 8u));
    }
    fixed_store_le32(c + 0xe318, 0xfffffffeu);
    fixed_store_le32(c + 0xe2a8, 0xffffffffu);
    fixed_store_le16(c + 0x17f98, 0x1ff);
    fixed_store_le32(c + 0x1dfc0, 0x1ff);
    for (i = 0; i < 0x200u; ++i)
        fixed_store_le32(c + 0x1cfc0 + i * 4u, i);
    fixed_store_le32(c + 0x1f4dc, 0);
    fixed_store_le32(c + 0x1879c, 0x1ff01ff);
    fixed_store_le16(c + 0x17f9a, 0x1ff);
    fixed_store_le32(c + 0x1f4a8, 1);
    fixed_store_le32(c + 0x1f4b0, 1);
    fixed_store_le32(c + 0x1f500, 0);
    return control;
}

void dovi_InitMdsExt(const void *configuration, void *extensions)
{
    unsigned char *p = (unsigned char *)extensions;
    (void)configuration;
    fixed_store_le32(p + 0x8c, 0);
    fixed_store_le32(p + 0x118, 0);
    fixed_store_le64(p + 0x84, 0x700000002ull);
    fixed_store_le64(p + 0x110, 0x1500000008ull);
    fixed_store_le32(p + 0x220, 0);
    fixed_store_le64(p + 0x218, 0xc0000000aull);
}

void dovi_InitTmCtrl(void *tone_control)
{
    unsigned char *tone = (unsigned char *)tone_control;
    dovi_control_os.memset(tone, 0, 0x158);
    fixed_store_le64(tone + 0x14, 0);
    fixed_store_le64(tone + 4, 0x20000000200ull);
    fixed_store_le64(tone + 12, 0x20000000200ull);
    fixed_store_le64(tone + 0x24, 0x600000001000ull);
    fixed_store_le64(tone + 0x40, 0x100000001000ull);
    fixed_store_le64(tone + 0x48, 0);
    fixed_store_le32(tone + 0x30, 0x1000);
    fixed_store_le64(tone + 0x68, 0xfffff80000000800ull);
    fixed_store_le64(tone + 0x70, 0xfffff80000000800ull);
    fixed_store_le32(tone + 0x78, 0x800);
    fixed_store_le64(tone + 0xd8, 0xfffff80000000800ull);
    fixed_store_le64(tone + 0xe0, 0xfffff80000000800ull);
    fixed_store_le32(tone + 0xe8, 0x800);
    fixed_store_le32(tone + 0x38, 0x1000);
    fixed_store_le32(tone + 0x8c, 0x1000);
    fixed_store_le64(tone + 0xf0, 0xfffff800fffff800ull);
    fixed_store_le64(tone + 0xf8, 0xffffff00ffffff00ull);
    fixed_store_le64(tone + 0x100, 0xffffff00ffffff00ull);
    fixed_store_le64(tone + 0x108, 0xffffff00ffffff00ull);
    fixed_store_le32(tone + 0x94, 0x1000);
    fixed_store_le64(tone + 0x98, 0x1000);
    fixed_store_le64(tone + 0x80, 0x100000000001ull);
    fixed_store_le64(tone + 0x110, 0xffffff80ffffff80ull);
    fixed_store_le64(tone + 0x118, 0xffffff80ffffff80ull);
    fixed_store_le64(tone + 0x120, 0xffffff80ffffff80ull);
    fixed_store_le64(tone + 0x128, 0x82100000001ull);
    fixed_store_le64(tone + 0x1c, 0);
    fixed_store_le64(tone + 0x50, 0);
    fixed_store_le64(tone + 0x58, 0);
    fixed_store_le32(tone + 0x60, 0);
    fixed_store_le64(tone + 0xb4, 0);
    fixed_store_le64(tone + 0xac, 0);
    fixed_store_le64(tone + 0xa4, 0);
    fixed_store_le64(tone + 0xcc, 0);
    fixed_store_le64(tone + 0xc4, 0);
    fixed_store_le64(tone + 0xbc, 0);
    fixed_store_le64(tone + 0x178, 0);
    fixed_store_le64(tone + 0x180, 0);
    fixed_store_le64(tone + 0x168, 0);
    fixed_store_le64(tone + 0x170, 0);
    fixed_store_le64(tone + 0x158, 0);
    fixed_store_le64(tone + 0x160, 0);
    fixed_store_le64(tone + 0x130, 0x10000003dull);
    fixed_store_le64(tone + 0x188, 0x400000000ull);
    fixed_store_le64(tone + 0x190, 0x200000000ull);
    fixed_store_le64(tone + 0x198, 0x200000000ull);
    fixed_store_le64(tone + 0x1a0, 0x200000000ull);
    fixed_store_le64(tone + 0x1a8, 0x200000000ull);
}

int dovi_GetStbTmmIdx(unsigned int index, void *min_pq, void *min, void *max_pq, void *max,
                      const void *control)
{
    const unsigned char *c = (const unsigned char *)control;
    if (index == 0 || c == 0)
        return -1;
    if (fixed_load_le32(c + 0x43b8) == fixed_load_le32(c + 0x43bc) ||
        fixed_load_le32(c + 12) != index) {
        if (index > 5u)
            limit_trap();
        c += (unsigned long long)index * 0x1f78u;
    }
    if (min_pq != 0)
        fixed_store_le16((unsigned char *)min_pq, fixed_load_le32(c + 0xe2ac));
    if (min != 0)
        fixed_store_le32((unsigned char *)min, fixed_load_le32(c + 0xe2b4));
    if (max_pq != 0)
        fixed_store_le16((unsigned char *)max_pq, fixed_load_le32(c + 0xe2b0));
    if (max != 0)
        fixed_store_le32((unsigned char *)max, fixed_load_le32(c + 0xe2b8));
    return 0;
}

int dovi_destroy_cp(void)
{
    return 0;
}

void dovi_register_content_info_callback(dovi_content_info_callback callback)
{
    dovi_content_info_cb_handle = callback;
}

void dovi_set_dovi2hdr10_mapping(void *context)
{
    fixed_store_le32((unsigned char *)context + 0x144c, 0);
}

void dovi_set_force_maxCLL_FALL_to_0(void *state, unsigned int value)
{
    fixed_store_le32((unsigned char *)state + 0x1438, value);
}

int dovi_set_L11_hdmi_tx(void *state, unsigned int enabled, unsigned int content_type,
                         unsigned int intended_white, unsigned int extra1, unsigned int extra2)
{
    unsigned char *p = (unsigned char *)state;
    fixed_store_le32(p + 0x7c, enabled);
    fixed_store_le32(p + 0x80, 1);
    fixed_store_le32(p + 0x84, content_type & 255u);
    fixed_store_le32(p + 0x8c, intended_white & 255u);
    fixed_store_le32(p + 0x90, extra1 & 255u);
    fixed_store_le32(p + 0x94, extra2 & 255u);
    return 0;
}

int dovi_set_i_dm_peak_stretch(void *state, unsigned int value)
{
    fixed_store_le32((unsigned char *)state + 0x778, value);
    return 0;
}

void *dovi_get_dm_ks_buf(const void *state, unsigned int index)
{
    if (index >= 4u)
        limit_trap();
    return (void *)fixed_load_le64((const unsigned char *)state + 0x28 +
                                   (unsigned long long)index * 8u);
}

void *dovi_get_dm_ctx_buf(const void *context)
{
    return (void *)fixed_load_le64((const unsigned char *)context);
}

int dovi_init_cp(void *arena, int instances, void *configuration)
{
    unsigned char *context = (unsigned char *)arena;
    unsigned int i;
    if (!context)
        return -1;
    dovi_InitCdmUtil();
    if (instances >= 5)
        return -1;
    fixed_store_le64(context + 0xd18, (unsigned long long)configuration);
    fixed_store_le32(context + 0x1440, 0);
    fixed_store_le64(context + 0xd28, ~0ull);
    fixed_store_le32(context + 0x144c, 1);
    if (instances > 0) {
        for (i = 0; i < (unsigned int)instances; ++i) {
            fixed_store_le64(context + 0x28 + i * 8,
                             (unsigned long long)(context + 0x2cd0 + i * 0x1608));
            fixed_store_le64(context + 8 + i * 8,
                             (unsigned long long)(context + 0x84f0 + i * 0x2bc));
        }
        fixed_store_le64(context, (unsigned long long)(context + 0x8fe0));
    } else
        fixed_store_le64(context, fixed_load_le64(context + 8) + 0xaf0);
    dovi_dm_InitDmCfg(0, context + 0x940, (unsigned long *)(context + 0x48));
    for (i = 0; instances > 0 && i < (unsigned int)instances; ++i)
        if (!dovi_dm_InitDmKs(0, context + 0x48, (void *)fixed_load_le64(context + 0x28 + i * 8)))
            return -1;
    if (!dovi_InitDm(instances, context + 0x28, (void *)fixed_load_le64(context)))
        return -1;
    for (i = 0; instances > 0 && i < (unsigned int)instances; ++i)
        dovi_InitMdsExt(context + 0x48,
                        (void *)fixed_load_le64(context + 8 + i * 8));
    dovi_InitTmCtrl(context + 0x750);
    fixed_store_le64(context + 0x6c, 0x1fd00000001ull);
    return 0;
}
