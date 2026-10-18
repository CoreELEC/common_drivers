/* Display-management configuration, metadata and kernel-state interfaces. */
#ifndef DOVI_DM_PIPELINE_H
#define DOVI_DM_PIPELINE_H
#include "dm_types.h"
#include "dm_input_map.h"
#include "dm_output_map.h"
#include "dm_trim.h"
#include "dm_configuration.h"
#include "dm_frame_format.h"
#include "dm_transfer.h"

/* Input display metadata decoding (dm_metadata.c). */
int dovi_dm_ParseMds(const unsigned char *input, unsigned int size, unsigned int *metadata,
                     int input_index, const void *configuration, long context);

/* Output display metadata selection and serialization (dm_metadata_output.c). */
int dovi_dm_GetMdsOutput(unsigned char *output, unsigned int capacity, unsigned int format,
                         unsigned int flags, unsigned int mode, void *context);
int dovi_dm_GenerateOutputMd(unsigned char *output, unsigned int capacity, unsigned int format,
                             unsigned int flags, int mode, void *context);

/* Luminance coordinates for trim interpolation (dm_trim_luminance.c). */
void dovi_dm_BuildLumaXInfo(int coordinate_scale, unsigned long unused_state, long config,
                            unsigned int flags, int exclude_default, long metadata, long signal,
                            int *trim_header, long coordinates, long indices,
                            unsigned int *last_index, int *target_coordinate);

/* Chroma coordinates and distances for trim interpolation (dm_trim_chroma.c). */
void dovi_dm_BuildChromaXInfo(int coordinate_scale, unsigned long unused_state, long config,
                              unsigned int flags, int exclude_default, long metadata, long signal,
                              int *trim_header, long coordinates, long indices,
                              unsigned int *last_index, unsigned int *target_coordinate);

/* Trim interpolation brackets and inverse lookup tables (dm_trim_interpolation.c). */
void dovi_dm_FindInterpIdx(int *coordinates, int last_index, int target, int *lower_index,
                           int *upper_index);
unsigned int dovi_dm_BuildInterpInfo(long config, long metadata, int *trim_header, int *coordinates,
                                     long indices, int last_index, int target,
                                     unsigned int *weight_q12, long default_trim, long *lower_trim,
                                     long *upper_trim);
void dovi_dm_MakeTheInverseLutUnique(long input_lut, unsigned int *output_lut, short *inverse_lut,
                                     int *last_index);

/* Display-management commit orchestration (dm_commit_pipeline.c). */
unsigned int dovi_dm_Commit2DmKs(unsigned int signal_index, unsigned int *config,
                                 unsigned int *metadata, unsigned int *context);

/* Consolidation of input metadata and color transforms (dm_input_consolidation.c). */
void dovi_dm_ConsolidateInput(unsigned int signal_index, int *config, unsigned int *flags,
                              unsigned char *metadata, long context);

/* Adaptive backlight updates and global dimming results (dm_backlight.c). */
void dovi_dm_ApplyAblu(unsigned int signal_index, long config, unsigned int flags,
                       unsigned char *metadata, unsigned int *context);
unsigned long dovi_dm_GetGdActiveFltrdAnchor(unsigned short *filtered_pq,
                                             unsigned int *filtered_linear, long context);
unsigned long dovi_dm_GetGdActiveFltrdMean(unsigned short *filtered_pq,
                                           unsigned int *filtered_linear, long context);
unsigned long dovi_dm_GetGdActiveTgtWindow(unsigned int *window_word_28,
                                           unsigned int *window_word_24, long context);
unsigned long dovi_dm_GetGdBackLightModelInput(unsigned int *output, long context);
unsigned long dovi_dm_GetGdBackLightModelOutput(unsigned int *output, long context);

/* Kernel-state initialization, configuration and finalization (dm_kernel_maps.c). */
unsigned short *dovi_dm_InitDmKs(unsigned long context_handle, const void *config,
                                 unsigned short *kernel_state);
unsigned int dovi_dm_CheckMds(const void *metadata, const void *previous);
unsigned long dovi_dm_CommitCfg(unsigned int signal_index, long config, unsigned char *context);
void dovi_dm_CommitFinalized(int *config, long context);

#endif
