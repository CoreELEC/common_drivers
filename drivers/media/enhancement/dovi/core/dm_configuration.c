/* Display-management signal environments and configuration. */
#include "dm_bindings.h"
#include "dm_layout.h"
#include "fixed_access.h"

void dovi_dm_InitSrcSigEnvPri(int *signal_words, int signal_index)
{
    struct dm_signal_environment *signal = (void *)signal_words;
    unsigned int primaries_scale;

    memset(signal_words + 4, 0, 0x110);
    signal->signal_index = signal_index;
    signal->color_space = 0;
    signal->chroma_format = 0;
    signal->weave = 0;
    signal->data_type = 0;
    signal->field_04 = 0;
    signal->height = 0x438;
    signal->width = 0x780;
    signal->bit_depth = 0xe;
    signal->primaries_scale = 0x1a;
    signal_words[0x1e] = 0xe;
    signal_words[0x20] = 0x8000;
    signal_words[0x21] = 0x8000;
    signal_words[0x22] = 0x8000;
    signal_words[0x23] = 0xf;
    signal_words[0x25] = 0x2806;
    signal_words[0x26] = 0x2a1c;
    if (signal_index != 0) {
        signal_words[0x1f] = 1;
    }
    signal_words[0xe] = 2;
    primaries_scale = dovi_GetPrimaries(0u, signal->primaries);
    signal_words[0x29] = 0;
    signal_words[0x2a] = 2;
    signal_words[0x27] = 0x51f;
    signal_words[0x28] = 4;
    signal->primaries_scale = primaries_scale;
    signal->field_114 = 0x18;
    signal->field_118 = -1;
    signal->eotf = 0;
    signal->gamma = 0x999a;
    signal->minimum = 0x51e;
    signal->maximum = 0x3e800000;
    signal_words[0x2b] = 0;
    signal_words[0x32] = 0;
    signal_words[0x39] = 0;
    signal->transfer_parameters[0] = 0;
    signal->transfer_parameters[1] = 0;
    signal->transfer_parameters[2] = 0;
    signal->transfer_parameters[3] = 0;
    signal->field_11c = 1;
    return;
}

void dovi_dm_InitSrcSigEnvSec(int *signal_words, int signal_index)
{
    struct dm_signal_environment *signal = (void *)signal_words;
    int *transfer_parameters;
    unsigned int primaries_scale;

    memset(signal_words + 4, 0, 0x110);
    signal->signal_index = signal_index;
    signal->color_space = 0;
    signal->chroma_format = 0;
    signal->weave = 0;
    signal->data_type = 0;
    signal->field_04 = 0;
    signal->height = 0x438;
    signal->width = 0x780;
    signal->bit_depth = 0xe;
    signal->primaries_scale = 0x1a;
    signal_words[0x1e] = 0xe;
    signal_words[0x20] = 0x8000;
    signal_words[0x21] = 0x8000;
    signal_words[0x22] = 0x8000;
    signal_words[0x23] = 0xf;
    signal_words[0x25] = 0x2806;
    signal_words[0x26] = 0x2a1c;
    if (signal_index != 0) {
        signal_words[0x1f] = 1;
    }
    signal_words[0xe] = 2;
    primaries_scale = dovi_GetPrimaries(0u, signal->primaries);
    signal->primaries_scale = primaries_scale;
    signal_words[0x29] = 0;
    signal_words[0x2a] = 2;
    signal_words[0x27] = 0x51f;
    signal_words[0x28] = 4;
    transfer_parameters = signal->transfer_parameters;
    signal->field_114 = 0x18;
    signal->field_118 = -1;
    signal->minimum = 0x51e;
    signal->maximum = 0x3e800000;
    signal_words[0x2b] = 0;
    signal->field_11c = 1;
    signal_words[0x32] = 0;
    signal_words[0x39] = 0;
    signal->eotf = 0;
    signal->gamma = 0x999a;
    transfer_parameters[0] = 0;
    transfer_parameters[1] = 0;
    signal->transfer_parameters[2] = 0;
    signal->transfer_parameters[3] = 0;
    signal->color_space = 2;
    signal->chroma_format = 2;
    signal->weave = 0;
    signal->data_type = 1;
    signal->bit_depth = 8;
    signal_words[0xd] = 2;
    primaries_scale = dovi_GetPrimaries(1u, signal->primaries);
    signal->eotf = 0;
    signal->gamma = 0x999a;
    signal->primaries_scale = primaries_scale;
    signal_words[0x28] = 4;
    signal_words[0x29] = 1;
    signal->minimum = 0x51e;
    signal->maximum = 0x1900000;
    transfer_parameters[0] = 0;
    transfer_parameters[1] = 0x4330181;
    signal->transfer_parameters[2] = 0x151cb4;
    return;
}

void dovi_dm_InitAlpFrmFmt(unsigned int *frame_words, unsigned int signal_index)
{
    *frame_words = signal_index;
    fixed_store_le64((unsigned char *)frame_words + 32, 0);
    fixed_store_le64((unsigned char *)frame_words + 40, 0);
    fixed_store_le64((unsigned char *)frame_words + 4, 0x43800000000);
    frame_words[3] = 0x780;
    frame_words[0xc] = 0;
    fixed_store_le64((unsigned char *)frame_words + 16, 0);
    fixed_store_le64((unsigned char *)frame_words + 24, 0x200000005);
    frame_words[9] = 1;
    frame_words[10] = 8;
    return;
}

void dovi_dm_InitAlpSigEnv(unsigned long *signal_words)
{
    signal_words[4] = 0;
    signal_words[5] = 0;
    *(unsigned int *)(signal_words + 6) = 0;
    *signal_words = 2;
    signal_words[1] = 0x78000000438;
    signal_words[2] = 0;
    signal_words[3] = 0x200000005;
    *(unsigned int *)((long)signal_words + 0x24) = 1;
    *(unsigned int *)(signal_words + 5) = 8;
    return;
}

void dovi_dm_InitTgtSigEnv(void *signal)
{
    unsigned int primaries_scale;

    memset(signal, 0, 0x120);
    *(unsigned int *)((long)signal + 0x5c) = 0x1a;
    fixed_store_le64((unsigned char *)signal + 8, 0x78000000438);
    *(unsigned int *)((long)signal + 0x28) = 0xe;
    *(unsigned int *)((long)signal + 0x78) = 0xe;
    fixed_store_le64((unsigned char *)signal + 0x18, 0);
    fixed_store_le64((unsigned char *)signal + 0x20, 0);
    fixed_store_le64((unsigned char *)signal + 0x80, 0x800000008000);
    fixed_store_le64((unsigned char *)signal + 0x88, 0xf00008000);
    fixed_store_le64((unsigned char *)signal + 0x94, 0x2a1c00002806);
    fixed_store_le64((unsigned char *)signal + 0x34, 0x200000000);
    primaries_scale = dovi_GetPrimaries((unsigned int)(0), (void *)((long)signal + 0x3c));
    fixed_store_le64((unsigned char *)signal + 0xa4, 0x200000000);
    *(unsigned int *)((long)signal + 0x5c) = primaries_scale;
    fixed_store_le64((unsigned char *)signal + 0x9c, 0x40000051f);
    fixed_store_le64((unsigned char *)signal + 0x114, 0xffffffff00000018);
    fixed_store_le64((unsigned char *)signal + 0xfc, 0x3e8000000000051e);
    *(unsigned int *)((long)signal + 0xac) = 0;
    *(unsigned int *)((long)signal + 200) = 0;
    *(unsigned int *)((long)signal + 0x11c) = 1;
    *(unsigned int *)((long)signal + 0xe4) = 0;
    fixed_store_le64((unsigned char *)signal + 0xf4, 0x999a00000000);
    fixed_store_le64((unsigned char *)signal + 0x104, 0);
    fixed_store_le64((unsigned char *)signal + 0x10c, 0);
    fixed_store_le64((unsigned char *)signal + 0x18, 0x100000000);
    fixed_store_le64((unsigned char *)signal + 0x20, 2);
    *(unsigned int *)((long)signal + 0x28) = 0xc;
    *(unsigned int *)((long)signal + 0x34) = 2;
    primaries_scale = dovi_GetPrimaries((unsigned int)(1), (void *)((long)signal + 0x3c));
    *(unsigned int *)((long)signal + 0xa8) = 2;
    fixed_store_le64((unsigned char *)signal + 0xa0, 4);
    fixed_store_le64((unsigned char *)signal + 0xf4, 0x999a00000000);
    *(unsigned int *)((long)signal + 0x5c) = primaries_scale;
    fixed_store_le64((unsigned char *)signal + 0xfc, 0x19000000000051e);
    fixed_store_le64((unsigned char *)signal + 0x104, 0x433018100000000);
    *(unsigned int *)((long)signal + 0x10c) = 0x151cb4;
    return;
}

unsigned long dovi_dm_GetSigEnv(void *output, unsigned int signal_index, long context)
{

    if (context == 0) {
        return 0xffffffff;
    }
    if (signal_index < DM_SIGNAL_COUNT) {
        dovi_MemCpyByte(output,
                   (const void *)(context + (unsigned long)signal_index * DM_SIGNAL_BYTES +
                                  DM_CONTEXT_SIGNALS_OFFSET),
                   (int)(DM_SIGNAL_BYTES));
        return 0;
    }
    dm_limit_trap();
}

long dovi_dm_GetSigEnvPtr(unsigned int signal_index, long context)
{

    if (context == 0) {
        return 0;
    }
    if (signal_index < DM_SIGNAL_COUNT) {
        return context + (unsigned long)signal_index * DM_SIGNAL_BYTES + DM_CONTEXT_SIGNALS_OFFSET;
    }
    dm_limit_trap();
}

int dovi_dm_GetTmCtrl(void *output, long context)
{
    if (context != 0) {
        dovi_MemCpyByte(output, (const void *)(context + DM_CONTEXT_TONE_CONTROL_OFFSET),
                   (int)(DM_TONE_CONTROL_BYTES));
        return 0;
    }
    return -1;
}

void dovi_dm_InitGdCtrl(unsigned long *control_words)
{
    control_words[2] = 0;
    control_words[3] = 0x100000001000;
    *control_words = 0x14000000000001;
    control_words[1] = 0x100000001000;
    return;
}

unsigned long dovi_dm_GetGdCtrl(void *output, long context)
{
    if (context != 0) {
        dovi_MemCpyByte(output, (const void *)(context + DM_CONTEXT_GD_CONTROL_OFFSET),
                   (int)(DM_GD_CONTROL_BYTES));
        return 0;
    }
    return 0xffffffff;
}

void dovi_dm_InitDmCtrl(unsigned long *control_words)
{
    control_words[6] = 0;
    control_words[7] = 0;
    control_words[8] = 0;
    control_words[9] = 0;
    *control_words = 0x100000001;
    control_words[1] = 0x100000000;
    control_words[4] = 0;
    control_words[5] = 0x100000000;
    control_words[10] = 0;
    control_words[0xb] = 0x100000001;
    *(unsigned int *)(control_words + 0xc) = 0;
    *(unsigned int *)((long)control_words + 0x34) = 1;
    control_words[2] = 0x100000002;
    control_words[3] = 0;
    return;
}

int dovi_dm_GetDmCtrl(void *output, long context)
{
    if (context != 0) {
        dovi_MemCpyByte(output, (const void *)(context + DM_CONTEXT_CONTROL_OFFSET),
                   (int)(DM_CONTROL_BYTES));
        return 0;
    }
    return -1;
}

unsigned long *dovi_dm_InitDmCfg(unsigned long unused, void *memory_manager,
                                 unsigned long *config_words)
{
    unsigned int primaries_scale;

    ((struct dm_configuration *)config_words)->memory_manager = memory_manager;
    config_words[6] = 0;
    config_words[7] = 0;
    config_words[8] = 0;
    config_words[9] = 0;
    *config_words = 0x100000001;
    config_words[1] = 0x100000000;
    config_words[4] = 0;
    config_words[5] = 0x100000000;
    config_words[2] = 0x100000002;
    config_words[3] = 0;
    config_words[10] = 0;
    config_words[0xb] = 0x100000001;
    *(unsigned int *)((long)config_words + 0x34) = 1;
    *(unsigned int *)(config_words + 0xc) = 0;
    memset((void *)((long)config_words + 0x194), 0, 0x110);
    *(unsigned int *)(config_words + 0x3c) = 0x1a;
    *(unsigned int *)(config_words + 0x42) = 0xf;
    fixed_store_le64((unsigned char *)config_words + 0x19c, 0);
    fixed_store_le64((unsigned char *)config_words + 0x1a4, 0);
    fixed_store_le64((unsigned char *)config_words + 0x184, 1);
    fixed_store_le64((unsigned char *)config_words + 0x18c, 0x78000000438);
    *(unsigned int *)((long)config_words + 0x1ac) = 0xe;
    *(unsigned int *)((long)config_words + 0x1fc) = 0xe;
    config_words[0x41] = 0x800000008000;
    config_words[0x43] = 0x2a1c00002806;
    config_words[0x40] = 0x800000000001;
    *(unsigned int *)((long)config_words + 0x1bc) = 2;
    primaries_scale = dovi_GetPrimaries((unsigned int)(0), (void *)(config_words + 0x38));
    *(unsigned int *)(config_words + 0x3c) = primaries_scale;
    config_words[0x45] = 0x200000000;
    config_words[0x44] = 0x40000051f;
    *(unsigned int *)(config_words + 0x46) = 0;
    config_words[0x4f] = 0x999a00000000;
    config_words[0x50] = 0x3e8000000000051e;
    *(unsigned int *)((long)config_words + 0x24c) = 0;
    config_words[0x53] = 0xffffffff00000018;
    *(unsigned int *)(config_words + 0x54) = 1;
    config_words[199] = 1;
    *(unsigned int *)(config_words + 0x4d) = 0;
    config_words[0x51] = 0;
    config_words[0x52] = 0;
    config_words[0xc9] = 0;
    config_words[0xca] = 0;
    config_words[0xcb] = 0;
    config_words[0xcc] = 0;
    config_words[200] = 0x78000000438;
    config_words[0xca] = 0x200000005;
    fixed_store_le64((unsigned char *)config_words + 0x65c, 0x800000001);
    *(unsigned int *)(config_words + 0xcd) = 0;
    memset((void *)((long)config_words + 0x2b4), 0, 0x110);
    fixed_store_le64((unsigned char *)config_words + 0x2a4, 2);
    fixed_store_le64((unsigned char *)config_words + 0x2ac, 0x78000000438);
    *(unsigned int *)(config_words + 0x60) = 0x1a;
    fixed_store_le64((unsigned char *)config_words + 700, 0);
    fixed_store_le64((unsigned char *)config_words + 0x2c4, 0);
    *(unsigned int *)(config_words + 0x66) = 0xf;
    *(unsigned int *)((long)config_words + 0x2cc) = 0xe;
    *(unsigned int *)((long)config_words + 0x31c) = 0xe;
    config_words[0x65] = 0x800000008000;
    *(unsigned int *)((long)config_words + 0x2dc) = 2;
    config_words[0x67] = 0x2a1c00002806;
    config_words[100] = 0x800000000001;
    primaries_scale = dovi_GetPrimaries((unsigned int)(0), (void *)(config_words + 0x5c));
    *(unsigned int *)(config_words + 0x60) = primaries_scale;
    config_words[0x69] = 0x200000000;
    config_words[0x68] = 0x40000051f;
    *(unsigned int *)(config_words + 0x6a) = 0;
    *(unsigned int *)((long)config_words + 0x36c) = 0;
    config_words[0x73] = 0x999a00000000;
    *(unsigned int *)(config_words + 0x71) = 0;
    config_words[0x75] = 0;
    config_words[0x76] = 0;
    config_words[0x74] = 0x3e8000000000051e;
    *(unsigned int *)(config_words + 0x78) = 1;
    fixed_store_le64((unsigned char *)config_words + 0x67c, 0);
    fixed_store_le64((unsigned char *)config_words + 0x684, 0);
    fixed_store_le64((unsigned char *)config_words + 0x68c, 0);
    fixed_store_le64((unsigned char *)config_words + 0x694, 0);
    config_words[0x77] = 0xffffffff00000018;
    fixed_store_le64((unsigned char *)config_words + 0x66c, 2);
    fixed_store_le64((unsigned char *)config_words + 0x674, 0x78000000438);
    fixed_store_le64((unsigned char *)config_words + 0x684, 0x200000005);
    *(unsigned int *)((long)config_words + 0x69c) = 0;
    config_words[0xd2] = 0x800000001;
    memset((void *)((long)config_words + 0x3d4), 0, 0x110);
    fixed_store_le64((unsigned char *)config_words + 0x3cc, 0x78000000438);
    *(unsigned int *)(config_words + 0x84) = 0x1a;
    fixed_store_le64((unsigned char *)config_words + 0x3dc, 0);
    fixed_store_le64((unsigned char *)config_words + 0x3e4, 0);
    *(unsigned int *)(config_words + 0x8a) = 0xf;
    fixed_store_le64((unsigned char *)config_words + 0x3c4, 3);
    *(unsigned int *)((long)config_words + 0x3ec) = 0xe;
    config_words[0x89] = 0x800000008000;
    *(unsigned int *)((long)config_words + 0x43c) = 0xe;
    *(unsigned int *)((long)config_words + 0x3fc) = 2;
    config_words[0x8b] = 0x2a1c00002806;
    config_words[0x88] = 0x800000000001;
    primaries_scale = dovi_GetPrimaries((unsigned int)(0), (void *)(config_words + 0x80));
    config_words[0x8d] = 0x200000000;
    *(unsigned int *)(config_words + 0x84) = primaries_scale;
    config_words[0x8c] = 0x40000051f;
    *(unsigned int *)(config_words + 0x8e) = 0;
    *(unsigned int *)((long)config_words + 0x48c) = 0;
    config_words[0x97] = 0x999a00000000;
    *(unsigned int *)(config_words + 0x95) = 0;
    config_words[0x99] = 0;
    config_words[0x9a] = 0;
    config_words[0x98] = 0x3e8000000000051e;
    *(unsigned int *)(config_words + 0x9c) = 1;
    config_words[0xd6] = 0;
    config_words[0xd7] = 0;
    config_words[0x9b] = 0xffffffff00000018;
    config_words[0xd8] = 0;
    config_words[0xd9] = 0;
    config_words[0xd4] = 3;
    config_words[0xd5] = 0x78000000438;
    config_words[0xd7] = 0x200000005;
    fixed_store_le64((unsigned char *)config_words + 0x6c4, 0x800000001);
    *(unsigned int *)(config_words + 0xda) = 0;
    memset((void *)((long)config_words + 0x4f4), 0, 0x110);
    fixed_store_le64((unsigned char *)config_words + 0x4ec, 0x78000000438);
    *(unsigned int *)(config_words + 0xa8) = 0x1a;
    fixed_store_le64((unsigned char *)config_words + 0x4fc, 0);
    fixed_store_le64((unsigned char *)config_words + 0x504, 0);
    *(unsigned int *)(config_words + 0xae) = 0xf;
    fixed_store_le64((unsigned char *)config_words + 0x4e4, 4);
    *(unsigned int *)((long)config_words + 0x50c) = 0xe;
    config_words[0xad] = 0x800000008000;
    *(unsigned int *)((long)config_words + 0x55c) = 0xe;
    *(unsigned int *)((long)config_words + 0x51c) = 2;
    config_words[0xaf] = 0x2a1c00002806;
    config_words[0xac] = 0x800000000001;
    primaries_scale = dovi_GetPrimaries((unsigned int)(0), (void *)(config_words + 0xa4));
    *(unsigned int *)(config_words + 0xa8) = primaries_scale;
    config_words[0xb1] = 0x200000000;
    config_words[0xb0] = 0x40000051f;
    *(unsigned int *)(config_words + 0xb2) = 0;
    *(unsigned int *)((long)config_words + 0x5ac) = 0;
    config_words[0xbb] = 0x999a00000000;
    *(unsigned int *)(config_words + 0xb9) = 0;
    config_words[0xbd] = 0;
    config_words[0xbe] = 0;
    config_words[0xbc] = 0x3e8000000000051e;
    fixed_store_le64((unsigned char *)config_words + 0x6e4, 0);
    fixed_store_le64((unsigned char *)config_words + 0x6ec, 0);
    fixed_store_le64((unsigned char *)config_words + 0x6f4, 0);
    fixed_store_le64((unsigned char *)config_words + 0x6fc, 0);
    *(unsigned int *)(config_words + 0xc0) = 1;
    fixed_store_le64((unsigned char *)config_words + 0x6d4, 4);
    fixed_store_le64((unsigned char *)config_words + 0x6dc, 0x78000000438);
    config_words[0xbf] = 0xffffffff00000018;
    config_words[0xdf] = 0x800000001;
    fixed_store_le64((unsigned char *)config_words + 0x6ec, 0x200000005);
    *(unsigned int *)((long)config_words + 0x704) = 0;
    memset((void *)((long)config_words + 100), 0, 0x120);
    *(unsigned int *)((long)config_words + 0x8c) = 0xe;
    *(unsigned int *)((long)config_words + 0xdc) = 0xe;
    fixed_store_le64((unsigned char *)config_words + 0xec, 0xf00008000);
    fixed_store_le64((unsigned char *)config_words + 0x6c, 0x78000000438);
    fixed_store_le64((unsigned char *)config_words + 0x7c, 0);
    fixed_store_le64((unsigned char *)config_words + 0xe4, 0x800000008000);
    fixed_store_le64((unsigned char *)config_words + 0x84, 0);
    *(unsigned int *)(config_words + 0x18) = 0x1a;
    *(unsigned int *)((long)config_words + 0x9c) = 2;
    config_words[0x1f] = 0x2a1c00002806;
    primaries_scale = dovi_GetPrimaries((unsigned int)(0), (void *)(config_words + 0x14));
    *(unsigned int *)(config_words + 0x18) = primaries_scale;
    *(unsigned int *)(config_words + 0x22) = 0;
    config_words[0x20] = 0x40000051f;
    config_words[0x21] = 0x200000000;
    *(unsigned int *)((long)config_words + 300) = 0;
    *(unsigned int *)(config_words + 0x29) = 0;
    config_words[0x2d] = 0;
    config_words[0x2e] = 0;
    config_words[0x2b] = 0x999a00000000;
    config_words[0x2c] = 0x3e8000000000051e;
    config_words[0x2f] = 0xffffffff00000018;
    fixed_store_le64((unsigned char *)config_words + 0x7c, 0x100000000);
    *(unsigned int *)(config_words + 0x30) = 1;
    fixed_store_le64((unsigned char *)config_words + 0x84, 2);
    *(unsigned int *)(config_words + 0x13) = 2;
    *(unsigned int *)((long)config_words + 0x8c) = 0xc;
    primaries_scale = dovi_GetPrimaries((unsigned int)(1), (void *)(config_words + 0x14));
    fixed_store_le64((unsigned char *)config_words + 0x104, 4);
    *(unsigned int *)(config_words + 0x18) = primaries_scale;
    *(unsigned int *)((long)config_words + 0x10c) = 2;
    config_words[0x2b] = 0x999a00000000;
    config_words[0x2c] = 0x19000000000051e;
    config_words[0x2d] = 0x433018100000000;
    *(unsigned int *)(config_words + 0x2e) = 0x151cb4;
    memset(config_words + 0xe1, 0, 0x158);
    *(unsigned int *)(config_words + 0xe7) = 0x1000;
    fixed_store_le64((unsigned char *)config_words + 0x72c, 0x600000001000);
    *(unsigned int *)(config_words + 0xe8) = 0x1000;
    *(unsigned int *)((long)config_words + 0x794) = 0x1000;
    fixed_store_le64((unsigned char *)config_words + 0x70c, 0x20000000200);
    config_words[0xee] = 0xfffff80000000800;
    config_words[0xef] = 0xfffff80000000800;
    config_words[0xfc] = 0xfffff80000000800;
    config_words[0xfd] = 0xfffff80000000800;
    fixed_store_le64((unsigned char *)config_words + 0x714, 0x20000000200);
    *(unsigned int *)(config_words + 0xf0) = 0x800;
    config_words[0xff] = 0xfffff800fffff800;
    config_words[0xf1] = 0x100000000001;
    *(unsigned int *)(config_words + 0xfe) = 0x800;
    config_words[0x100] = 0xffffff00ffffff00;
    config_words[0x101] = 0xffffff00ffffff00;
    config_words[0x102] = 0xffffff00ffffff00;
    config_words[0x107] = 0x10000003d;
    fixed_store_le64((unsigned char *)config_words + 0x71c, 0);
    config_words[0x103] = 0xffffff80ffffff80;
    config_words[0x104] = 0xffffff80ffffff80;
    config_words[0x105] = 0xffffff80ffffff80;
    fixed_store_le64((unsigned char *)config_words + 0x724, 0);
    config_words[0xe9] = 0x100000001000;
    config_words[0xea] = 0;
    config_words[0x106] = 0x82100000001;
    config_words[0xeb] = 0;
    config_words[0xec] = 0;
    *(unsigned int *)(config_words + 0xed) = 0;
    fixed_store_le64((unsigned char *)config_words + 0x79c, 0x100000001000);
    fixed_store_le64((unsigned char *)config_words + 0x7bc, 0);
    fixed_store_le64((unsigned char *)config_words + 0x7b4, 0);
    fixed_store_le64((unsigned char *)config_words + 0x7ac, 0);
    fixed_store_le64((unsigned char *)config_words + 0x7d4, 0);
    fixed_store_le64((unsigned char *)config_words + 0x7cc, 0);
    fixed_store_le64((unsigned char *)config_words + 0x7c4, 0);
    config_words[0x10c] = 0;
    config_words[0x10d] = 0;
    config_words[0x10e] = 0;
    config_words[0x10f] = 0;
    config_words[0x110] = 0;
    config_words[0x111] = 0;
    config_words[0x112] = 0x400000000;
    config_words[0x113] = 0x200000000;
    config_words[0x114] = 0x200000000;
    config_words[0x115] = 0x200000000;
    config_words[0x116] = 0x200000000;
    config_words[0x117] = 0x14000000000001;
    config_words[0x118] = 0x100000001000;
    config_words[0x119] = 0;
    config_words[0x11a] = 0x100000001000;
    return config_words;
}
