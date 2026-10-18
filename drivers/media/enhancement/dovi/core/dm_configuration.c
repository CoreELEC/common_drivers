/* Display-management signal environments and configuration. */
#include "dm_pipeline.h"
#include "dm_configuration.h"
#include "control_abi_recovered.h"
#include "control_state.h"
#include "dm_bindings.h"
#include "byte_utils.h"
#include "pq_conversion.h"
#include "dm_commit_tables.inc"

void commit_chroma_resampling(DmCtxtFxp_t *context, DmCfgFxp_t *config)
{
    long long next_coefficient_offset;
    unsigned int target_chroma;
    unsigned char kernel_chroma;
    DmKsFxp_t *kernel;
    long long coefficient_byte_offset;
    unsigned int target_bit_depth;

    target_chroma = (config->tgtSigEnv).Chrm;
    kernel_chroma = '\0';
    kernel = context->hDmKs;
    if ((target_chroma != CChrm420) && (kernel_chroma = '\x02', target_chroma == CChrm422)) {
        kernel_chroma = '\x01';
    }
    (kernel->ksDs).chrmOut = kernel_chroma;
    (kernel->ksDs).minDs = 0;
    target_bit_depth = (config->tgtSigEnv).Bdp;
    (kernel->ksDs).filterUvColDsRadius = 7;
    (kernel->ksDs).filterUvColDsScale2P = 0xc;
    (kernel->ksDs).maxDs = (short)(1u << (unsigned long long)(target_bit_depth & 0x1f)) + -1;
    coefficient_byte_offset = 0;
    do {
        next_coefficient_offset = coefficient_byte_offset + 2;
        *(unsigned short *)((long long)(kernel->ksDs).filterUvColDs_m + coefficient_byte_offset) =
            *(unsigned short *)((long long)FilterUvColDs_m + coefficient_byte_offset);
        coefficient_byte_offset = next_coefficient_offset;
    } while (next_coefficient_offset != 0x1e);
    dovi_CommitKsUs((config->srcSigEnv).Chrm, &(config->srcSigEnv).Bdp, &context->hDmKs->ksUs);
    dovi_CommitKsUs((config->grcSigEnv).Chrm, &(config->grcSigEnv).Bdp, &context->hDmKs->ksUsG);
}

void dovi_dm_InitAlpSigEnv(SigEnvCom_t *signal)
{
    signal->RowNum = 1080;
    signal->ColNum = 1920;
    signal->Dtp = CDtpU8;
    signal->Clr = CClrGrey;
    signal->Weav = CWeavPlnr;
    signal->Chrm = CChrm444;
    signal->Loc = CLocHost;
    signal->Bdp = 8;
}

void dovi_dm_InitDmCtrl(DmCtrlFxp_t *control)
{
    control->StbMode = CStbModeGpri;
    control->PMin = 0x51e;
    control->PMinPq = 0;
    control->PMax = 0x1900000;
    control->PMaxPq = 0;
    control->Platform = CPlatformCpu;
    control->DevId = 0;
    control->Prf = 0;
    control->MdsFollowed = 0;
}

int dovi_dm_GetGrcSigEnv(GrcSigEnvFxp_t *output, DmCtxtFxp_t *context)
{
    DmCfgFxp_t *configuration = dm_selected_configuration(context);
    if (configuration == 0)
        return -1;
    dovi_MemCpyByte(output, &configuration->grcSigEnv, 0xe4);
    return 0;
}

int dovi_dm_GetSrcSigEnv(SrcSigEnvFxp_t *output, DmCtxtFxp_t *context)
{
    DmCfgFxp_t *configuration = dm_selected_configuration(context);
    if (configuration == 0)
        return -1;
    dovi_MemCpyByte(output, &configuration->srcSigEnv, 0xe4);
    return 0;
}

int dovi_dm_GetTgtSigEnv(TgtSigEnvFxp_t *output, DmCtxtFxp_t *context)
{
    DmCfgFxp_t *configuration = dm_selected_configuration(context);
    if (configuration == 0)
        return -1;
    dovi_MemCpyByte(output, &configuration->tgtSigEnv, 0xcc);
    return 0;
}

int dovi_dm_GetTmCtrl(TmCtrlFxp_t *output, DmCtxtFxp_t *context)
{
    DmCfgFxp_t *configuration = dm_selected_configuration(context);
    if (configuration == 0)
        return -1;
    dovi_MemCpyByte(output, &configuration->tmCtrl, 0xa8);
    return 0;
}

int dovi_dm_GetDmCtrl(DmCtrlFxp_t *output, DmCtxtFxp_t *context)
{
    DmCfgFxp_t *configuration = dm_selected_configuration(context);
    if (configuration == 0)
        return -1;
    dovi_MemCpyByte(output, &configuration->dmCtrl, 0x68);
    return 0;
}

GrcSigEnvFxp_t *dovi_dm_GetGrcSigEnvPtr(DmCtxtFxp_t *context)
{
    DmCfgFxp_t *configuration = dm_selected_configuration(context);
    return configuration != 0 ? &configuration->grcSigEnv : 0;
}

SrcSigEnvFxp_t *dovi_dm_GetSrcSigEnvPtr(DmCtxtFxp_t *context)
{
    DmCfgFxp_t *configuration = dm_selected_configuration(context);
    return configuration != 0 ? &configuration->srcSigEnv : 0;
}

TgtSigEnvFxp_t *dovi_dm_GetTgtSigEnvPtr(DmCtxtFxp_t *context)
{
    DmCfgFxp_t *configuration = dm_selected_configuration(context);
    return configuration != 0 ? &configuration->tgtSigEnv : 0;
}

int dovi_dm_GetStbGmm(unsigned short *minimum_pq, unsigned int *minimum, unsigned short *maximum_pq,
                      unsigned int *maximum, DmCtxtFxp_t *context)
{
    if (context == 0 || context->dmCfgActValid == 0)
        return -1;
    if (minimum_pq != 0)
        *minimum_pq = context->tc2Ctrl.tcSrcSig.minPq;
    if (minimum != 0)
        *minimum = dovi_PQ12ToL(context->tc2Ctrl.tcSrcSig.minPq);
    if (maximum_pq != 0)
        *maximum_pq = context->tc2Ctrl.tcSrcSig.maxPq;
    if (maximum != 0)
        *maximum = dovi_PQ12ToL(context->tc2Ctrl.tcSrcSig.maxPq);
    return 0;
}

void dovi_dm_InitSrcSigEnvSec(void *signal)
{
    fixed_store_le32((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_CLR_OFFSET), 2u);
    fixed_store_le32((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_CHRM_OFFSET), 2u);
    fixed_store_le32((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_RGBDEF_OFFSET), 2u);
    fixed_store_le32((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_YUVXFERSPEC_OFFSET), 2u);
    fixed_store_le16((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_GAMMA_OFFSET), 0x999au);
    fixed_store_le32((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_MIN_OFFSET), 0x51eu);
    fixed_store_le32((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_MAX_OFFSET), 0x1900000u);
    fixed_store_le16((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_A_OFFSET), 0x181u);
    fixed_store_le32((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_ROWNUM_OFFSET), 1080u);
    fixed_store_le16((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_B_OFFSET), 0x433u);
    fixed_store_le32((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_COLNUM_OFFSET), 1920u);
    fixed_store_le32((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_DTP_OFFSET), 1u);
    fixed_store_le32((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_RNG_OFFSET), 1u);
    fixed_store_le32((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_BDP_OFFSET), 8u);
    fixed_store_le32((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_V3WP_OFFSET + 8u), 0x8000u);
    fixed_store_le32((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_V3WP_OFFSET + 4u), 0x8000u);
    fixed_store_le32((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_V3WP_OFFSET), 0x8000u);
    fixed_store_le32((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_G_OFFSET), 0x151cb4u);
    fixed_store_le16((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_WPSCALE_OFFSET), 15u);
    fixed_store_le16((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_DIAGSIZE_OFFSET), 42u);
    fixed_store_le32((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_WEAV_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_CROSSTALK_OFFSET), 0x28fu);
    fixed_store_le32((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_LOC_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_EOTF_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_RGB2LMSRGBWEXT_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_RGB2LMSM33EXT_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_WPEXT_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_YUV2RGBEXT_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_YUV2RGBOFFEXT_OFFSET), 0u);
    fixed_store_le16((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_MINPQ_OFFSET), 0u);
    fixed_store_le16((unsigned char *)signal + (DM_GRAPHICS_SIGNAL_MAXPQ_OFFSET), 0u);
}

void dovi_dm_InitSrcSigEnvPri(void *signal)
{
    fixed_store_le32((unsigned char *)signal + (DM_SOURCE_SIGNAL_V3WP_OFFSET + 8u), 0x8000u);
    fixed_store_le32((unsigned char *)signal + (DM_SOURCE_SIGNAL_V3WP_OFFSET + 4u), 0x8000u);
    fixed_store_le32((unsigned char *)signal + (DM_SOURCE_SIGNAL_V3WP_OFFSET), 0x8000u);
    fixed_store_le16((unsigned char *)signal + (DM_SOURCE_SIGNAL_WPSCALE_OFFSET), 15u);
    fixed_store_le32((unsigned char *)signal + (DM_SOURCE_SIGNAL_YUVXFERSPEC_OFFSET), 2u);
    fixed_store_le16((unsigned char *)signal + (DM_SOURCE_SIGNAL_GAMMA_OFFSET), 0x999au);
    fixed_store_le32((unsigned char *)signal + (DM_SOURCE_SIGNAL_MIN_OFFSET), 0x51eu);
    fixed_store_le32((unsigned char *)signal + (DM_SOURCE_SIGNAL_MAX_OFFSET), 0x3e800000u);
    fixed_store_le16((unsigned char *)signal + (DM_SOURCE_SIGNAL_A_OFFSET), 0x3dfbu);
    fixed_store_le32((unsigned char *)signal + (DM_SOURCE_SIGNAL_ROWNUM_OFFSET), 1080u);
    fixed_store_le16((unsigned char *)signal + (DM_SOURCE_SIGNAL_B_OFFSET), 0xe4u);
    fixed_store_le32((unsigned char *)signal + (DM_SOURCE_SIGNAL_COLNUM_OFFSET), 1920u);
    fixed_store_le16((unsigned char *)signal + (DM_SOURCE_SIGNAL_DIAGSIZE_OFFSET), 42u);
    fixed_store_le32((unsigned char *)signal + (DM_SOURCE_SIGNAL_DTP_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_SOURCE_SIGNAL_WEAV_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_SOURCE_SIGNAL_LOC_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_SOURCE_SIGNAL_CLR_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_SOURCE_SIGNAL_BDP_OFFSET), 14u);
    fixed_store_le32((unsigned char *)signal + (DM_SOURCE_SIGNAL_EOTF_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_SOURCE_SIGNAL_RNG_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_SOURCE_SIGNAL_RGBDEF_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_SOURCE_SIGNAL_RGB2LMSRGBWEXT_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_SOURCE_SIGNAL_RGB2LMSM33EXT_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_SOURCE_SIGNAL_WPEXT_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_SOURCE_SIGNAL_YUV2RGBEXT_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_SOURCE_SIGNAL_YUV2RGBOFFEXT_OFFSET), 0u);
    fixed_store_le16((unsigned char *)signal + (DM_SOURCE_SIGNAL_MINPQ_OFFSET), 0u);
    fixed_store_le16((unsigned char *)signal + (DM_SOURCE_SIGNAL_MAXPQ_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_SOURCE_SIGNAL_G_OFFSET), 0x151cb4u);
    fixed_store_le32((unsigned char *)signal + (DM_SOURCE_SIGNAL_CROSSTALK_OFFSET), 0x28fu);
    /* Leave Chrm and all row-pitch/matrix fields. */
}

void dovi_dm_InitTgtSigEnv(void *signal)
{
    fixed_store_le32((unsigned char *)signal + (DM_TARGET_SIGNAL_WEAV_OFFSET), 2u);
    fixed_store_le32((unsigned char *)signal + (DM_TARGET_SIGNAL_RGBDEF_OFFSET), 2u);
    fixed_store_le32((unsigned char *)signal + (DM_TARGET_SIGNAL_YUVXFERSPEC_OFFSET), 2u);
    fixed_store_le32((unsigned char *)signal + (DM_TARGET_SIGNAL_ROWNUM_OFFSET), 1080u);
    fixed_store_le32((unsigned char *)signal + (DM_TARGET_SIGNAL_MIN_OFFSET), 0x51eu);
    fixed_store_le32((unsigned char *)signal + (DM_TARGET_SIGNAL_COLNUM_OFFSET), 1920u);
    fixed_store_le32((unsigned char *)signal + (DM_TARGET_SIGNAL_V3WP_OFFSET + 8u), 0x8000u);
    fixed_store_le32((unsigned char *)signal + (DM_TARGET_SIGNAL_V3WP_OFFSET + 4u), 0x8000u);
    fixed_store_le32((unsigned char *)signal + (DM_TARGET_SIGNAL_V3WP_OFFSET), 0x8000u);
    fixed_store_le32((unsigned char *)signal + (DM_TARGET_SIGNAL_MAX_OFFSET), 0x1900000u);
    fixed_store_le32((unsigned char *)signal + (DM_TARGET_SIGNAL_CHRM_OFFSET), 1u);
    fixed_store_le16((unsigned char *)signal + (DM_TARGET_SIGNAL_WPSCALE_OFFSET), 15u);
    fixed_store_le16((unsigned char *)signal + (DM_TARGET_SIGNAL_DIAGSIZE_OFFSET), 42u);
    fixed_store_le32((unsigned char *)signal + (DM_TARGET_SIGNAL_DTP_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_TARGET_SIGNAL_LOC_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_TARGET_SIGNAL_BDP_OFFSET), 12u);
    fixed_store_le32((unsigned char *)signal + (DM_TARGET_SIGNAL_CLR_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_TARGET_SIGNAL_EOTF_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_TARGET_SIGNAL_RNG_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_TARGET_SIGNAL_LMS2RGBRGBWEXT_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_TARGET_SIGNAL_LMS2RGBM33EXT_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_TARGET_SIGNAL_WPEXT_OFFSET), 0u);
    fixed_store_le16((unsigned char *)signal + (DM_TARGET_SIGNAL_GAMMA_OFFSET), 0x999au);
    fixed_store_le32((unsigned char *)signal + (DM_TARGET_SIGNAL_RGB2YUVEXT_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_TARGET_SIGNAL_RGB2YUVOFFEXT_OFFSET), 0u);
    fixed_store_le16((unsigned char *)signal + (DM_TARGET_SIGNAL_MINPQ_OFFSET), 0u);
    fixed_store_le16((unsigned char *)signal + (DM_TARGET_SIGNAL_MAXPQ_OFFSET), 0u);
    fixed_store_le32((unsigned char *)signal + (DM_TARGET_SIGNAL_CROSSTALK_OFFSET), 0x28fu);
}

void *dovi_dm_InitDmCfg(int platform, void *h_mmg, void *config)
{
    unsigned char *bytes = (unsigned char *)config;
    void *alpha = bytes + DM_CONFIG_ALPSIGENV_OFFSET;
    (void)platform;
    fixed_store_le64((unsigned char *)config + (DM_CONFIG_HMMG_OFFSET), (unsigned long long)h_mmg);
    /* DmCtrlFxp_t scalar offsets and widths. */
    fixed_store_le32((unsigned char *)config + (32u), 1u); /* CFxp */
    fixed_store_le32((unsigned char *)config + (12u), 0x51eu);
    fixed_store_le32((unsigned char *)config + (8u), 0u); /* CStbModeGpri */
    fixed_store_le32((unsigned char *)config + (16u), 0x1900000u);
    fixed_store_le16((unsigned char *)config + (20u), 0u);
    fixed_store_le16((unsigned char *)config + (22u), 0u);
    fixed_store_le32((unsigned char *)config + (24u), 0u); /* CPlatformCpu */
    fixed_store_le32((unsigned char *)config + (28u), 0u);
    fixed_store_le32((unsigned char *)config + (96u), 0u);
    fixed_store_le32((unsigned char *)config + (100u), 0u);
    dovi_dm_InitSrcSigEnvSec(bytes + DM_CONFIG_GRCSIGENV_OFFSET);
    /* Alpha scalar layout is the common signal prefix with Bdp at +36. */
    fixed_store_le32((unsigned char *)alpha + (28u), 1u); /* CDtpU8 */
    fixed_store_le32((unsigned char *)alpha + (4u), 1920u);
    fixed_store_le32((unsigned char *)alpha + (16u), 4u); /* CClrGrey */
    fixed_store_le32((unsigned char *)alpha + (0u), 1080u);
    fixed_store_le32((unsigned char *)alpha + (20u), 2u); /* CChrm444 */
    fixed_store_le32((unsigned char *)alpha + (24u), 0u); /* CWeavPlnr */
    fixed_store_le32((unsigned char *)alpha + (36u), 8u);
    fixed_store_le32((unsigned char *)alpha + (32u), 0u); /* CLocHost */
    dovi_dm_InitSrcSigEnvPri(bytes + DM_CONFIG_SRCSIGENV_OFFSET);
    dovi_dm_InitTgtSigEnv(bytes + DM_CONFIG_TGTSIGENV_OFFSET);
    dovi_InitTmCtrl(bytes + DM_CONFIG_TMCTRL_OFFSET);
    return config;
}
