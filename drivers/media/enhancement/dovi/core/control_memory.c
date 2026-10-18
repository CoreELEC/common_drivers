/* Memory requirements and pointer assignment for the control arena. */
#include "control_memory.h"
#include "dm_frame_format.h"
#include "fixed_access.h"

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

#define ALIGN8 __declspec(align(8))
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

#define ALIGN8 __attribute__((aligned(8)))
#endif

ALIGN8 unsigned char dovi_DisplayIdxTbl[400];
ALIGN8 const unsigned char dovi_DisplayTbl[140] = {
    0x01,0x00,0x00,0x00,0x1e,0x05,0x00,0x00,0x00,0x00,0x90,0x01,0x45,0x08,0x02,0x00,0x01,0x00,0x00,0x00,
    0x1b,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x60,0x09,0xf4,0xc8,0x02,0x00,0x00,0x00,0x00,0x00,
    0x1c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x60,0x09,0xf4,0xc8,0x02,0x00,0x02,0x00,0x00,0x00,
    0x25,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x1f,0x33,0x4f,0x03,0x00,0x00,0x00,0x00,0x00,
    0x26,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x1f,0x33,0x4f,0x03,0x00,0x02,0x00,0x00,0x00,
    0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xa0,0x0f,0xce,0x01,0x03,0x00,0x00,0x00,0x00,0x00,
    0x31,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xa0,0x0f,0xce,0x01,0x03,0x00,0x02,0x00,0x00,0x00
};

void *dovi_InitMmg(void *manager)
{
    unsigned char *manager_bytes = (unsigned char *)manager;
    fixed_store_le32(manager_bytes + 0x3d4, 0x1608u);
    fixed_store_le64(manager_bytes + 0x3c4, 0x2bc0001f938ull);
    return manager;
}

int dovi_RegMemReq(unsigned int size, unsigned int alignment, unsigned int in_device,
                   void *parameter, control_memory_callback callback, void *manager)
{
    unsigned char *manager_bytes = (unsigned char *)manager, *record;
    unsigned int count = fixed_load_le32(manager_bytes + 0x3c0);
    if (count >= 24u && count < 0x80000000u) {
        dovi_control_os.printk("\001" "0### ASSERTION FAILED %s: %s: %d: %s\n",
                               "../dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmMmg.c",
                               "RegMemReq", 34, "hMmg->reqNum < MMG_REQ_ITEM_MAX");
        dovi_control_os.dump_stack();
        count = fixed_load_le32(manager_bytes + 0x3c0);
        if (count == 24u)
            return -1;
    }
    if (count >= 25u)
        limit_trap();
    record = manager_bytes + (unsigned long long)count * 32u;
    fixed_store_le32(record + 8, in_device);
    fixed_store_le32(record, size);
    fixed_store_le32(record + 4, alignment == 0 ? 4u : alignment);
    count = fixed_load_le32(manager_bytes + 0x3c0);
    fixed_store_le64(record + 16, (unsigned long long)parameter);
    fixed_store_le64(record + 24, (unsigned long long)callback);
    if (count >= 24u)
        limit_trap();
    fixed_store_le64(manager_bytes + 0x300 + (unsigned long long)count * 8u, 0);
    fixed_store_le32(manager_bytes + 0x3c0, fixed_load_le32(manager_bytes + 0x3c0) + 1u);
    return 0;
}

int dovi_MemAssignCbFunFc(void *pointer_field, void *buffer)
{
    fixed_store_le64((unsigned char *)pointer_field, (unsigned long long)buffer);
    return 0;
}

void dovi_MemZeroByte(void *destination, unsigned long long count)
{
    if (count != 0)
        dovi_control_os.memset(destination, 0, count);
}

int dovi_UpdateCompactFrmMemRowPitch(void *frame)
{
    unsigned char *f = (unsigned char *)frame;
    unsigned int height = fixed_load_le32(f + 8), width = fixed_load_le32(f + 12),
                 format = fixed_load_le32(f + 24);
    unsigned int chroma = fixed_load_le32(f + 28), packed = fixed_load_le32(f + 32),
                 type = fixed_load_le32(f + 36);
    unsigned int bytes = dm_frame_sample_bytes(type), luma_pitch, luma_size, chroma_pitch,
                 chroma_size;
    if (packed != 0) {
        luma_pitch = dm_frame_interleaved_components(format, chroma) * width * bytes;
        fixed_store_le32(f + 16, luma_pitch);
        fixed_store_le32(f + 20, 0);
        return fixed_signed32(luma_pitch * height);
    }
    luma_pitch = bytes * width;
    luma_size = luma_pitch * height;
    fixed_store_le32(f + 16, luma_pitch);
    if (format == 5u) {
        fixed_store_le32(f + 20, 0);
        return fixed_signed32(luma_size);
    }
    chroma_pitch = bytes * fixed_asr32(width, 1);
    chroma_size = chroma_pitch * fixed_asr32(height, 1);
    if (chroma != 0) {
        chroma_pitch = luma_pitch;
        chroma_size = luma_size;
    }
    fixed_store_le32(f + 20, chroma_pitch);
    return fixed_signed32(format == 2u ? (chroma_size + luma_size) * 2u
                                       : luma_size + chroma_size * 2u);
}

unsigned int dovi_GetMinFrmMemSize(unsigned int height, unsigned int width,
                                   unsigned int color_space, unsigned int chroma_format,
                                   unsigned int weave, unsigned int data_type)
{
    unsigned int sample_bytes = dm_frame_sample_bytes(data_type), luma_size, chroma_size;
    if (weave != 0)
        sample_bytes *= dm_frame_interleaved_components(color_space, chroma_format);
    luma_size = width * height * sample_bytes;
    /* The signed division rounds toward zero after 32-bit wrapping. */
    chroma_size = (unsigned int)(fixed_signed32(luma_size) / 4);
    if (chroma_format != 0)
        chroma_size = luma_size;
    if (color_space != DM_FRAME_COLOR_GRAY && weave == 0)
        luma_size = color_space == DM_FRAME_COLOR_RGBA ? (chroma_size + luma_size) * 2u
                                                     : luma_size + chroma_size * 2u;
    return luma_size;
}

int dovi_SyncIoFrmFmtWithMds(const void *metadata, void *frame)
{
    const unsigned char *m = (const unsigned char *)metadata;
    unsigned char *f = (unsigned char *)frame;
    unsigned int value, first, second;
    if (m == 0 || (m[0] & 1u) == 0)
        return 0;
    value = m[0x49] == 0 ? 0u : m[0x49] == 1 ? 1u : 3u;
    fixed_store_le32(f + 0x18, value);
    if (value == 3u) {
        first = fixed_load_le16(m + 0x2a);
        first |= 0u - (first & 0x8000u);
        second = fixed_load_le16(m + 0x2c);
        second |= 0u - (second & 0x8000u);
        if (((first * 3u + second * 97u) >> 31) != 0)
            fixed_store_le32(f + 0x18, 4);
    }
    value = m[0x4a] == 0 ? 0u : m[0x4a] == 1 ? 1u : 2u;
    fixed_store_le32(f + 0x1c, value);
    value = m[0x4a] == 1 ? 2u : 0u;
    fixed_store_le32(f + 0x20, value);
    fixed_store_le32(f + 0x24, m[0x48] < 9u);
    fixed_store_le32(f + 0x28, m[0x48]);
    return 0;
}

unsigned int dovi_GetRunMode(unsigned int index, const void *control)
{
    const unsigned char *c = (const unsigned char *)control;
    if (fixed_load_le32(c + 0x43b8) != fixed_load_le32(c + 0x43bc) &&
        fixed_load_le32(c + 12) == index)
        return fixed_load_le32(c + 0xe318);
    if (index >= 6u)
        limit_trap();
    return fixed_load_le32(c + 0xe318 + (unsigned long long)index * 0x1f78u);
}

unsigned int dovi_GetActiveVs10RunMode(const void *control)
{
    return fixed_load_le32((const unsigned char *)control + 0x43bc);
}

void *dovi_GetHDmCfg(void *control)
{
    return control != 0 ? (void *)((unsigned long long)control + 8u) : (void *)0;
}

void *dovi_GetHDmKs(unsigned int index, const void *control)
{
    unsigned long long address =
        (unsigned long long)control + fixed_sign32(index - 1u) * 8u + 0x1f918u;
    return (void *)fixed_load_le64((const unsigned char *)address);
}

int dovi_UpdateKsInFrmMemRowPitch(unsigned int luma, unsigned int chroma, unsigned int index,
                                  void *control)
{
    const unsigned char *slot;
    if (control == 0)
        return -1;
    slot = (const unsigned char *)((unsigned long long)control + fixed_sign32(index - 1u) * 8u +
                                   0x1f918u);
    fixed_store_le32((unsigned char *)fixed_load_le64(slot) + 0xcc, luma);
    fixed_store_le32((unsigned char *)fixed_load_le64(slot) + 0xd4, chroma);
    return 0;
}

int dovi_InitCdmUtil(void)
{
    fixed_store_le64(dovi_DisplayIdxTbl + 8, (unsigned long long)dovi_DisplayTbl);
    fixed_store_le64(dovi_DisplayIdxTbl + 0xd8, (unsigned long long)(dovi_DisplayTbl + 20));
    fixed_store_le64(dovi_DisplayIdxTbl + 0xe0, (unsigned long long)(dovi_DisplayTbl + 40));
    fixed_store_le64(dovi_DisplayIdxTbl + 0x128, (unsigned long long)(dovi_DisplayTbl + 60));
    fixed_store_le64(dovi_DisplayIdxTbl + 0x130, (unsigned long long)(dovi_DisplayTbl + 80));
    fixed_store_le64(dovi_DisplayIdxTbl + 0x180, (unsigned long long)(dovi_DisplayTbl + 100));
    fixed_store_le64(dovi_DisplayIdxTbl + 0x188, (unsigned long long)(dovi_DisplayTbl + 120));
    return 0;
}

unsigned long long dovi_init_cp_mmg(int instances)
{
    unsigned char manager[0x3d8] = {0};
    dovi_InitMmg(manager);
    return (unsigned int)(fixed_load_le32(manager + 0x3c4) +
                          (fixed_load_le32(manager + 0x3d4) + fixed_load_le32(manager + 0x3c8)) *
                              (unsigned int)instances) +
           0x2cd0ull;
}
