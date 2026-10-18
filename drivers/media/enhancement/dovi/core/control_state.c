/* Persistent control-state initialization and access. */
#include "control_state.h"
#include "fixed_access.h"
#include "dm_configuration.h"
#include "dm_pipeline.h"
#include "pq_conversion.h"
#include "byte_utils.h"
typedef char require_widths[(sizeof(unsigned int) == 4 && sizeof(unsigned long long) == 8 &&
                             sizeof(void *) == 8)
                                ? 1
                                : -1];

void *dovi_get_dm_ctx_buf(const void *context)
{
    return (unsigned char *)context + CONTROL_CONTEXT_BYTES;
}

void dovi_set_graphics_offset(void *context, int minimum, int maximum)
{
    unsigned char *state = context;
    fixed_store_le32(state + CONTROL_CONTEXT_GRAPHIC_MIN_OFFSET_OFFSET, (unsigned int)minimum);
    fixed_store_le32(state + CONTROL_CONTEXT_GRAPHIC_MAX_OFFSET_OFFSET, (unsigned int)maximum);
}

void *dovi_InitDm(const void *config, void *dm)
{
    (void)config;
    fixed_store_le32((unsigned char *)dm + DM_CONTEXT_DMCFGTOBEVALID_OFFSET, 0);
    fixed_store_le32((unsigned char *)dm + DM_CONTEXT_DMCFGACTVALID_OFFSET, 0);
    fixed_store_le64((unsigned char *)dm + DM_CONTEXT_PDMCFGACT_OFFSET,
                     (unsigned long long)((unsigned char *)dm + DM_CONTEXT_DMCFG_OFFSET));
    fixed_store_le64(
        (unsigned char *)dm + DM_CONTEXT_PDMCFGTOBE_OFFSET,
        (unsigned long long)((unsigned char *)dm + DM_CONTEXT_DMCFG_OFFSET + DM_CONFIG_BYTES));
    fixed_store_le32((unsigned char *)dm + DM_CONTEXT_MDSEXTFRCCMMT_OFFSET, 0);
    fixed_store_le32((unsigned char *)dm + DM_CONTEXT_MDSEXTACTVALID_OFFSET, 0);
    return dm;
}

int dovi_init_cp(void *arena)
{
    unsigned char *context = (unsigned char *)arena;
    void *config = context + CONTROL_CONTEXT_DM_CFG_OFFSET;
    void *kernel = context + CONTROL_CONTEXT_BYTES;
    void *extensions = context + CONTROL_CONTEXT_BYTES + DM_KERNEL_BYTES;
    void *dm = (unsigned char *)extensions + DM_EXTENSIONS_BYTES;
    fixed_store_le32(context + CONTROL_CONTEXT_LAST_INPUT_FORMAT_OFFSET, 0xffffffffu);
    fixed_store_le32(context + CONTROL_CONTEXT_LAST_OUTPUT_FORMAT_OFFSET, 0xffffffffu);
    fixed_store_le32(context + CONTROL_CONTEXT_SUPPORT_EL_OFFSET, 0);
    fixed_store_le32(context + CONTROL_CONTEXT_LAST_GRAPHIC_ON_OFFSET, 0);
    fixed_store_le32(context + CONTROL_CONTEXT_DOVI2HDR10_NOMAPPING_OFFSET, 1);
    fixed_store_le64(context + CONTROL_CONTEXT_H_KS_OFFSET, (unsigned long long)kernel);
    fixed_store_le64(context + CONTROL_CONTEXT_H_MDS_EXT_OFFSET, (unsigned long long)extensions);
    fixed_store_le64(context + CONTROL_CONTEXT_H_DM_OFFSET, (unsigned long long)dm);
    (void)dovi_dm_InitDmCfg(0, context + CONTROL_CONTEXT_MMG_OFFSET, config);
    fixed_store_le32(
        (unsigned char *)(config) + (DM_CONFIG_SRCSIGENV_OFFSET + DM_SOURCE_SIGNAL_EOTF_OFFSET), 1);
    if (!dovi_dm_InitDmKs(config, kernel))
        return -1;
    dm = (void *)fixed_load_le64((const unsigned char *)context + CONTROL_CONTEXT_H_DM_OFFSET);
    if (!dovi_InitDm(config, dm))
        return -1;
    extensions =
        (void *)fixed_load_le64((const unsigned char *)context + CONTROL_CONTEXT_H_MDS_EXT_OFFSET);
    dovi_InitMdsExt(config, extensions);
    dovi_InitTmCtrl((unsigned char *)config + DM_CONFIG_TMCTRL_OFFSET);
    return 0;
}

int dovi_destroy_cp(void)
{
    return 0;
}

void dovi_set_dovi2hdr10_mapping(void *context)
{
    fixed_store_le32((unsigned char *)context + CONTROL_CONTEXT_DOVI2HDR10_NOMAPPING_OFFSET, 0);
}

void dovi_set_support_el(void *context, int enabled)
{
    fixed_store_le32((unsigned char *)context + CONTROL_CONTEXT_SUPPORT_EL_OFFSET, enabled != 0);
}

void dovi_InitTmCtrl(void *tone_control)
{
    unsigned char *tone = tone_control;
    unsigned int index;
    fixed_store_le32(tone + DM_TONE_CONTROL_ROLLOFF_OFFSET, 0x2aaaaaaau);
    fixed_store_le32(tone + DM_TONE_CONTROL_KEYWEIGHT_OFFSET, 0x3000u);
    fixed_store_le32(tone + DM_TONE_CONTROL_INTENSITYVECTORWEIGHT_OFFSET, 0x6000u);
    fixed_store_le32(tone + DM_TONE_CONTROL_CHRMVECTORWEIGHT_OFFSET, 0x6000u);
    fixed_store_le32(tone + DM_TONE_CONTROL_CHRMVECTORWEIGHTBWD_OFFSET, 0x6000u);
    fixed_store_le32(tone + DM_TONE_CONTROL_BWDINVERTIBLE_OFFSET, 0u);
    fixed_store_le32(tone + DM_TONE_CONTROL_INTENSITYVECTORWEIGHTBWD_OFFSET, 0x14000u);
    fixed_store_le32(tone + DM_TONE_CONTROL_BPWEIGHT_OFFSET, 0x1000u);
    fixed_store_le16(tone + DM_TONE_CONTROL_TMIDBIAS_OFFSET, 0u);
    fixed_store_le16(tone + DM_TONE_CONTROL_TMAXBIAS_OFFSET, 0u);
    fixed_store_le16(tone + DM_TONE_CONTROL_TMINBIAS_OFFSET, 0u);
    fixed_store_le16(tone + DM_TONE_CONTROL_DBRIGHTNESS_OFFSET, 0u);
    fixed_store_le16(tone + DM_TONE_CONTROL_DCONTRAST_OFFSET, 0u);
    for (index = 1u; index < 5u; ++index)
        fixed_store_le32(tone + (DM_TONE_CONTROL_DEFAULT2_OFFSET + index * 4u), 0u);
    fixed_store_le32(tone + (DM_TONE_CONTROL_DEFAULT2_OFFSET + 20u), 0x1000u);
    fixed_store_le32(tone + (DM_TONE_CONTROL_DEFAULT2_OFFSET + 24u), 0x800u);
    for (index = 0u; index < 7u; ++index)
        fixed_store_le32(tone + (DM_TONE_CONTROL_VALUEADJ2_OFFSET + index * 4u), 0u);
    fixed_store_le32(tone + DM_TONE_CONTROL_VALUEADJ2SATGAINBWD_OFFSET, 0u);
    for (index = 1u; index < 5u; ++index)
        fixed_store_le32(tone + (DM_TONE_CONTROL_CODEBIAS2_OFFSET + index * 4u), 0xfffff800u);
    fixed_store_le32(tone + (DM_TONE_CONTROL_CODEBIAS2_OFFSET + 24u), 0u);
    fixed_store_le32(tone + (DM_TONE_CONTROL_CODEBIAS2_OFFSET + 20u), 0x800u);
    for (index = 0u; index < 4u; ++index) {
        fixed_store_le32(tone + (DM_TONE_CONTROL_DEFAULT3_OFFSET + index * 4u), 0u);
        fixed_store_le32(tone + (DM_TONE_CONTROL_CODEBIAS3_OFFSET + index * 4u), 0u);
    }
    /* Default2[0], CodeBias2[0], l2off and padding remain untouched. */
}

void dovi_InitMdsExt(const void *config, void *extensions)
{
    const unsigned char *tone = (const unsigned char *)config + DM_CONFIG_TMCTRL_OFFSET;
    unsigned char *ext = (unsigned char *)extensions;
    unsigned int index;
    /* Nested TrimSet_t starts at +80: u16 count; TrimSets[2] at +8,
     * each Trim_t has four u16 scalar words and a u64 pointer; storage +40.
     * Each descriptor occupies 16 bytes with eight-byte pointer alignment. */
    fixed_store_le16((unsigned char *)ext + DM_EXTENSIONS_MID_PQ_OFFSET, 0x7ffu);
    fixed_store_le16((unsigned char *)ext + DM_EXTENSIONS_MIN_PQ_OFFSET, 0u);
    fixed_store_le16((unsigned char *)ext + 88u, 2u);
    fixed_store_le16((unsigned char *)ext + DM_EXTENSIONS_MAX_PQ_OFFSET, 0xfffu);
    fixed_store_le16((unsigned char *)ext + DM_EXTENSIONS_TRIMSETS_OFFSET, 0u);
    fixed_store_le16((unsigned char *)ext + 90u, 7u);
    fixed_store_le16((unsigned char *)ext + 92u, 16u);
    fixed_store_le64((unsigned char *)ext + 96u, (unsigned long long)(ext + 120u));
    fixed_store_le16((unsigned char *)ext + 94u, 0u);
    fixed_store_le16((unsigned char *)ext + 120u, 0xfffu);
    for (index = 1u; index < 7u; ++index)
        fixed_store_le16((unsigned char *)ext + (120u + index * 2u),
                         fixed_load_le32((const unsigned char *)tone +
                                         (DM_TONE_CONTROL_DEFAULT2_OFFSET + index * 4u)) -
                             fixed_load_le32((const unsigned char *)tone +
                                             (DM_TONE_CONTROL_CODEBIAS2_OFFSET + index * 4u)));
    fixed_store_le16((unsigned char *)ext + 104u, 3u);
    fixed_store_le64((unsigned char *)ext + 112u, (unsigned long long)(ext + 358u));
    fixed_store_le16((unsigned char *)ext + 110u, 0u);
    fixed_store_le16((unsigned char *)ext + 106u, 4u);
    fixed_store_le16((unsigned char *)ext + 108u, 16u);
    fixed_store_le16((unsigned char *)ext + 358u, 0xfffu);
    for (index = 1u; index < 4u; ++index)
        fixed_store_le16((unsigned char *)ext + (358u + index * 2u),
                         fixed_load_le32((const unsigned char *)tone +
                                         (DM_TONE_CONTROL_DEFAULT3_OFFSET + index * 4u)) -
                             fixed_load_le32((const unsigned char *)tone +
                                             (DM_TONE_CONTROL_CODEBIAS3_OFFSET + index * 4u)));
}

int dovi_GetDmCfg(void *config, const void *dm_handle)
{
    const unsigned char *handle = (const unsigned char *)dm_handle, *source;
    unsigned int pending;
    if (!handle)
        return -1;
    pending = fixed_load_le32(handle + DM_CONTEXT_DMCFGTOBEVALID_OFFSET);
    if (pending == 0 && fixed_load_le32(handle + DM_CONTEXT_DMCFGACTVALID_OFFSET) == 0)
        return -1;
    source = (const unsigned char *)(void *)fixed_load_le64(
        handle + (pending != 0 ? DM_CONTEXT_PDMCFGTOBE_OFFSET : DM_CONTEXT_PDMCFGACT_OFFSET));
    if (!source)
        return -1;
    /* dovi_MemCpyByte copies forward even when the input and output overlap. */
    dovi_MemCpyByte(config, source, DM_CONFIG_BYTES);
    return 0;
}

int dovi_GetStbCc(unsigned short *min_pq, unsigned int *min_linear, unsigned short *max_pq,
                  unsigned int *max_linear, const void *dm_handle)
{
    const unsigned char *handle = (const unsigned char *)dm_handle;
    if (!handle || fixed_load_le32(handle + DM_CONTEXT_DMCFGACTVALID_OFFSET) == 0)
        return -1;
    if (min_pq)
        *min_pq = fixed_load_le16(handle + 0xa94);
    if (min_linear)
        *min_linear = dovi_PQ12ToL(fixed_load_le16(handle + 0xa94));
    if (max_pq)
        *max_pq = fixed_load_le16(handle + 0xa96);
    if (max_linear)
        *max_linear = dovi_PQ12ToL(fixed_load_le16(handle + 0xa96));
    return 0;
}
