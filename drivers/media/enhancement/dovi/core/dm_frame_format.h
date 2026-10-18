#ifndef DOVI_DM_FRAME_FORMAT_H
#define DOVI_DM_FRAME_FORMAT_H

enum dm_frame_values {
    DM_FRAME_COLOR_RGBA = 2,
    DM_FRAME_COLOR_GRAY = 4,
    DM_FRAME_CHROMA_422 = 1,
    DM_FRAME_SAMPLE_U8 = 1,
    DM_FRAME_SAMPLE_F32 = 2
};

static inline unsigned int dm_frame_sample_bytes(unsigned int data_type)
{
    if (data_type == DM_FRAME_SAMPLE_U8)
        return 1;
    return data_type == DM_FRAME_SAMPLE_F32 ? 4 : 2;
}

static inline unsigned int dm_frame_interleaved_components(unsigned int color, unsigned int chroma)
{
    if (color == DM_FRAME_COLOR_RGBA)
        return 4;
    if (color == DM_FRAME_COLOR_GRAY)
        return 1;
    return chroma == DM_FRAME_CHROMA_422 ? 2 : 3;
}

#include "dm_layout.h"

unsigned int dovi_GetCompactFrmMemLayout(int height, int width, unsigned int color,
                                         unsigned int chroma, unsigned int weave,
                                         unsigned int data_type, int *row_pitch,
                                         int *chroma_row_pitch);
int dovi_dm_UpdateSrcEnvFrameMemLayout(DmCtxtFxp_t *context);
int dovi_dm_UpdateKsInFrameMemLayout(int row_pitch, int chroma_row_pitch, DmCtxtFxp_t *context);
void dovi_dm_CommitSecIn(int secondary_input, DmCtxtFxp_t *context);
void dovi_dm_CommitCdm2KsFrmFmt(const void *signal, void *format);
#endif
