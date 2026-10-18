#ifndef DOVI_TONE_CONTROLS_H
#define DOVI_TONE_CONTROLS_H
struct dm_consolidation_workspace;
/* Cached state pointers retain the consolidation store/reload sequence. */
void dovi_prepare_tone_control(unsigned int signal_index, int *config, unsigned int *flags,
                               unsigned char *metadata, long context, unsigned int *color_state,
                               struct dm_consolidation_workspace *work, unsigned int *input_state,
                               unsigned int *tone_lut_last, int previous_mode,
                               unsigned int metadata_flags);
#endif
