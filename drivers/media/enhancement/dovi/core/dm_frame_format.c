/* Frame-format layout and parameters. */
#include "dm_frame_format.h"
#include "fixed_access.h"

unsigned int dovi_GetCompactFrmMemLayout(int height, int width, unsigned int color,
                                         unsigned int chroma, unsigned int weave,
                                         unsigned int data_type, int *row_pitch,
                                         int *chroma_row_pitch)
{
    unsigned int bytes = dm_frame_sample_bytes(data_type);
    unsigned int luma_pitch, luma_size, chroma_size;
    if (weave != CWeavPlnr) {
        luma_pitch = dm_frame_interleaved_components(color, chroma) * bytes * (unsigned int)width;
        *row_pitch = fixed_signed32(luma_pitch);
        *chroma_row_pitch = 0;
        return luma_pitch * (unsigned int)height;
    }
    luma_pitch = bytes * (unsigned int)width;
    *row_pitch = fixed_signed32(luma_pitch);
    luma_size = luma_pitch * (unsigned int)height;
    if (chroma == CChrm420) {
        unsigned int chroma_pitch = bytes * (unsigned int)(width >> 1);
        *chroma_row_pitch = fixed_signed32(chroma_pitch);
        chroma_size = chroma_pitch * (unsigned int)(height >> 1);
    } else {
        *chroma_row_pitch = fixed_signed32(luma_pitch);
        chroma_size = luma_size;
    }
    return color == CClrRgba ? (chroma_size + luma_size) * 2u : chroma_size * 2u + luma_size;
}

int dovi_dm_UpdateSrcEnvFrameMemLayout(DmCtxtFxp_t *context)
{
    SrcSigEnvFxp_t *source;
    if (context == 0 || context->dmCfgActValid == 0)
        return -1;
    source = &context->pDmCfgAct->srcSigEnv;
    dovi_GetCompactFrmMemLayout(source->RowNum, source->ColNum, source->Clr, source->Chrm,
                                source->Weav, source->Dtp, &source->RowPitch, &source->RowPitchC);
    return 0;
}

int dovi_dm_UpdateKsInFrameMemLayout(int row_pitch, int chroma_row_pitch, DmCtxtFxp_t *context)
{
    if (context == 0 || context->hDmKs == 0)
        return -1;
    context->hDmKs->ksFrmFmtI.rowPitch = row_pitch;
    context->hDmKs->ksFrmFmtI.rowPitchC = chroma_row_pitch;
    return 0;
}

void dovi_dm_CommitSecIn(int secondary_input, DmCtxtFxp_t *context)
{
    if (context != 0) {
        if (context->dmCfgToBeValid != 0)
            context->pDmCfgToBe->dmCtrl.SecIn = 0;
        context->hDmKs->ksDmCtrl.secIn = secondary_input != 0;
    }
}

void dovi_dm_CommitCdm2KsFrmFmt(const void *signal, void *format)
{
    unsigned int dtype, weave, channels, size;
    fixed_store_le32((unsigned char *)format + (0u),
                     fixed_load_le32((const unsigned char *)signal + (0u)));
    fixed_store_le32((unsigned char *)format + (4u),
                     fixed_load_le32((const unsigned char *)signal + (4u)));
    dtype = fixed_load_le32((const unsigned char *)signal + (28u));
    ((unsigned char *)format)[8] = dtype == 0u ? 0 : dtype == 1u ? 1 : 2;
    weave = fixed_load_le32((const unsigned char *)signal + (24u));
    ((unsigned char *)format)[9] = weave == 0u ? 0 : weave == 1u ? 1 : 2;
    ((unsigned char *)format)[10] =
        (unsigned char)(fixed_load_le32((const unsigned char *)signal + (32u)) != 0u);
    ((unsigned char *)format)[11] =
        (unsigned char)fixed_load_le32((const unsigned char *)signal + (36u));
    fixed_store_le32((unsigned char *)format + (16u),
                     fixed_load_le32((const unsigned char *)signal + (8u)));
    fixed_store_le32((unsigned char *)format + (24u),
                     fixed_load_le32((const unsigned char *)signal + (12u)));
    weave = fixed_load_le32((const unsigned char *)signal + (24u));
    channels = weave == 0u                                                    ? 1u
               : fixed_load_le32((const unsigned char *)signal + (16u)) == 2u ? 4u
               : fixed_load_le32((const unsigned char *)signal + (16u)) == 4u ? 1u
               : fixed_load_le32((const unsigned char *)signal + (20u)) == 1u ? 2u
                                                                              : 3u;
    dtype = fixed_load_le32((const unsigned char *)signal + (28u));
    size = dtype == 1u ? 1u : dtype == 2u ? 4u : 2u;
    fixed_store_le32((unsigned char *)format + (20u), channels * size);
}
