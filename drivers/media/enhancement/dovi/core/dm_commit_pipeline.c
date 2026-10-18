/* Display-management commit orchestration. */
#include "dm_pipeline.h"
#include "dm_bindings.h"
#include "byte_utils.h"
#include "pq_conversion.h"

int dovi_dm_CommitMds(MdsExt_t *metadata, DmCtxtFxp_t *context)
{
    DmCfgFxp_t *config;
    DmKsFxp_t *kernel;
    unsigned int changed;

    if ((metadata == (MdsExt_t *)0x0) && (context->mdsExtFrcCmmt != 0)) {
        metadata = (MdsExt_t *)0x0;
        changed = 0;
        if (context->mdsExtActValid != 0) {
            metadata = &context->mdsExtAct;
            changed = (unsigned int)(metadata != (MdsExt_t *)0x0);
        }
    } else {
        changed = (unsigned int)(metadata != (MdsExt_t *)0x0);
    }
    (context->pDmCfgToBe->dmCtrl).MdsFollowed = changed;
    kernel = context->hDmKs;
    config = context->pDmCfgToBe;
    if (context->dmCfgToBeValid == 0) {
        changed = 0;
        if (context->dmCfgActValid == 0) {
            return -1;
        }
    } else {
        commit_color_matrices(context, config);
        (kernel->ksDmCtrl).mainIn = 1;
        (kernel->ksDmCtrl).secIn = (config->dmCtrl).SecIn;
        (kernel->ksDmCtrl).prf = (config->dmCtrl).Prf;
        commit_graphics_input(context, config, kernel);
        commit_video_input(context, config, kernel);
        changed = commit_configuration_tone_maps(context, config, kernel);
        commit_chroma_resampling(context, config);
        context->dmCfgToBeValid = 0;
        context->pDmCfgToBe = context->pDmCfgAct;
        context->pDmCfgAct = config;
        context->dmCfgActValid = 1;
    }
    if (metadata == (MdsExt_t *)0x0) {
        return changed;
    }
    return commit_dynamic_metadata(metadata, context, changed);
}
