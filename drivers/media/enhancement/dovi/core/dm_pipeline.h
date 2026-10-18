/* Display-management configuration, metadata and kernel-state interfaces. */
#ifndef DOVI_DM_PIPELINE_H
#define DOVI_DM_PIPELINE_H

/* Objects cover metadata 520, configuration 984, DM 4400 and kernel
 * parameters 10928 bytes. Naturally aligned, distinct backing is required. */
#include "dm_layout.h"
#include "dm_input_map.h"
#include "dm_output_map.h"
#include "dm_trim.h"

void *dovi_dm_InitDmKs(const void *configuration, void *kernel);
unsigned int dovi_dm_CheckMds(const void *metadata, const void *previous);
/* The pending pointer is read before the nominal null-context check. Kernel
 * storage may be null only when configuration is unchanged. */
int dovi_dm_CommitCfg(const void *configuration, void *kernel, void *context);
int dovi_dm_CommitMds(MdsExt_t *metadata, DmCtxtFxp_t *context);

void commit_color_matrices(DmCtxtFxp_t *context, DmCfgFxp_t *configuration);
unsigned int commit_configuration_tone_maps(DmCtxtFxp_t *context, DmCfgFxp_t *configuration,
                                            DmKsFxp_t *kernel);
void commit_chroma_resampling(DmCtxtFxp_t *context, DmCfgFxp_t *configuration);
int commit_dynamic_metadata(MdsExt_t *metadata, DmCtxtFxp_t *context, unsigned int changes);

/* Decode the packed base and extensions, retaining fields written before a
 * length failure. Trim arrays are supplied by the caller and are not bounded
 * by this routine; their pointers, dimensions and counts are reloaded. */
int dovi_dm_ParseMds(const unsigned char *input, int size, MdsExt_t *metadata,
                     int output_window_mode, const DmCfgFxp_t *configuration);
#endif
