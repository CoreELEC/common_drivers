/* Kernel-state initialization and configuration commit. */
#include "dm_pipeline.h"
#include "control_memory.h"
#include "control_abi_recovered.h"
#include "fixed_access.h"
#include "byte_utils.h"

void *dovi_dm_InitDmKs(const void *config, void *kernel)
{
    void *memory;
    static const unsigned int sizes[] = {
        0x530u, 0xd36u, 0xd34u, 0x532u, 0x1a1cu, 0x2222u, 0x2220u, 0x1a1eu
    };
    unsigned int i;
    if (!config || !kernel)
        return 0;
    memory = (void *)fixed_load_le64((const unsigned char *)config + DM_CONFIG_HMMG_OFFSET);
    if (!memory)
        return 0;
    /* rowPitchNum is a ushort; its following padding remains untouched. */
    fixed_store_le16((unsigned char *)kernel + DM_KERNEL_ROWPITCHNUM_OFFSET, 0x1000u);
    (void)dovi_RegMemReq(
        0x32a0000u, 16u, fixed_load_le32((const unsigned char *)(config) + (0x18u)) != 0,
        (unsigned char *)kernel + DM_KERNEL_FRMBUF0_OFFSET, dovi_MemAssignCbFunFc, memory);
    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i)
        fixed_store_le16((unsigned char *)(kernel) + (sizes[i]), 511u);
    return kernel;
}

unsigned int dovi_dm_CheckMds(const void *metadata, const void *previous)
{
    if (dovi_MemEqByte(metadata, previous, DM_METADATA_COMPARE_BYTES))
        return MDS_CHG_NONE;
    return dovi_MemEqByte(metadata, previous, DM_METADATA_CONFIG_COMPARE_BYTES) ? MDS_CHG_TC
                                                                              : MDS_CHG_CFG;
}

int dovi_dm_CommitCfg(const void *config, void *kernel, void *dm)
{
    unsigned char *dst = (unsigned char *)(void *)fixed_load_le64((const unsigned char *)(dm) +
                                                                  (DM_CONTEXT_PDMCFGTOBE_OFFSET));
    unsigned long long frame;
    if (!dm)
        return -1;
    dovi_MemCpyByte(dst, config, 984);
    if (fixed_load_le32((const unsigned char *)(dst) + (8u)) == 1u) {
        fixed_store_le32((unsigned char *)(dst) + (952u),
                         fixed_load_le32((const unsigned char *)(dst) + (572u)));
        fixed_store_le32((unsigned char *)(dst) + (956u),
                         fixed_load_le32((const unsigned char *)(dst) + (576u)));
        fixed_store_le16((unsigned char *)(dst) + (960u),
                         fixed_load_le16((const unsigned char *)(dst) + (580u)));
        fixed_store_le16((unsigned char *)(dst) + (962u),
                         fixed_load_le16((const unsigned char *)(dst) + (582u)));
    }
    if (fixed_load_le32((const unsigned char *)(dm) + (4u)) != 0u &&
        dovi_MemEqByte(dst, (void *)fixed_load_le64((const unsigned char *)(dm) + (1976u)), 984) !=
            0)
        return 0;
    fixed_store_le64((unsigned char *)(dm) + (4392u), (unsigned long long)(kernel));
    fixed_store_le32((unsigned char *)(dm) + (0u), 1u);
    fixed_store_le32((unsigned char *)(dm) + (1992u), 1u);
    frame = (unsigned long long)(void *)fixed_load_le64((const unsigned char *)(kernel) + (8u));
    fixed_store_le64((unsigned char *)(kernel) + (16u),
                     (unsigned long long)((void *)(frame + 0x10e0000u)));
    fixed_store_le64((unsigned char *)(kernel) + (24u),
                     (unsigned long long)((void *)(frame + 0x21c0000u)));
    frame = (unsigned long long)(void *)fixed_load_le64((const unsigned char *)(kernel) + (32u));
    fixed_store_le64((unsigned char *)(kernel) + (40u),
                     (unsigned long long)((void *)(frame + 0x10e0000u)));
    fixed_store_le64((unsigned char *)(kernel) + (48u),
                     (unsigned long long)((void *)(frame + 0x21c0000u)));
    return 1;
}
