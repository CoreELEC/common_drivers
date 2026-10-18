/* Frame-format layout and parameters. */
#include "dm_bindings.h"
#include "dm_layout.h"

void dovi_dm_CommitCdm2KsFrmFmt(long signal, unsigned int flags, unsigned int *frame_words)
{
    const struct dm_signal_environment *environment = (const void *)signal;
    struct dm_frame_format *format = (void *)frame_words;
    unsigned int data_type;
    unsigned int weave;
    unsigned int components;

    format->height = (unsigned int)environment->height;
    format->width = (unsigned int)environment->width;
    format->location = environment->location != 0;
    format->row_pitch = (unsigned int)environment->row_pitch;
    format->chroma_row_pitch = (unsigned int)environment->chroma_row_pitch;
    if ((flags & 2u) != 0) {
        return;
    }
    data_type = (unsigned int)environment->data_type;
    format->data_type = data_type == 0u ? 0 : data_type == 1u ? 1 : 2;
    weave = (unsigned int)environment->weave;
    format->weave = weave == 0u ? 0 : weave == 1u ? 1 : 2;
    format->bit_depth = (char)(unsigned int)environment->bit_depth;
    components = 1u;
    if (environment->weave != 0) {
        if (environment->color_space == DM_FRAME_COLOR_RGBA)
            components = 4u;
        else if (environment->color_space != DM_FRAME_COLOR_GRAY)
            components = environment->chroma_format == DM_FRAME_CHROMA_422 ? 2u : 3u;
    }
    /* Reload after the preceding writes: overlapping views remain observable. */
    data_type = (unsigned int)environment->data_type;
    format->column_pitch = dm_frame_sample_bytes(data_type) * components;
}

void dovi_dm_CommitMds2KsFrmFmt(unsigned char *metadata, long frame)
{
    const struct dm_metadata_format *source = (const void *)metadata;
    struct dm_frame_format *format = (void *)frame;
    char weave;
    unsigned char bit_depth;
    int bytes_per_sample;
    int column_pitch;

    if ((metadata != 0) && ((source->flags & 1) != 0)) {
        bit_depth = source->bit_depth;
        format->data_type = bit_depth < 9;
        weave = '\0';
        if (source->chroma_format != 0) {
            weave = (source->chroma_format == 1) << 1;
        }
        format->weave = weave;
        format->bit_depth = source->bit_depth;
        if (weave == '\0') {
            column_pitch = 1;
            if (8 < bit_depth) {
                column_pitch = 2;
            }
        } else {
            column_pitch = 2;
            if (source->chroma_format != 1) {
                column_pitch = 3;
            }
            bytes_per_sample = 1;
            if (8 < bit_depth) {
                bytes_per_sample = 2;
            }
            column_pitch = column_pitch * bytes_per_sample;
        }
        format->column_pitch = column_pitch;
    }
    return;
}
