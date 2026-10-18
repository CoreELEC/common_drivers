/* Input color-space and transfer maps. */
#ifndef DOVI_DM_INPUT_MAP_H
#define DOVI_DM_INPUT_MAP_H
#include "dm_layout.h"

void dovi_dm_CommitIpt2KsIMap(unsigned int bits, unsigned int range, void *map);

void dovi_dm_CommitMdsCsc(MdsExt_t *metadata, DmCfgFxp_t *configuration, CscCtrlFxp_t *color,
                          DmKsIMapFxp_t *input_map);
void commit_graphics_input(DmCtxtFxp_t *context, DmCfgFxp_t *configuration, DmKsFxp_t *kernel);
void commit_video_input(DmCtxtFxp_t *context, DmCfgFxp_t *configuration, DmKsFxp_t *kernel);
#endif
