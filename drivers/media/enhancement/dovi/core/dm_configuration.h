/* Signal environments and configuration defaults. */
#ifndef DOVI_DM_CONFIGURATION_H
#define DOVI_DM_CONFIGURATION_H
#include "dm_layout.h"

/* Pending configuration takes precedence, including a null pending pointer. */
static inline DmCfgFxp_t *dm_selected_configuration(DmCtxtFxp_t *context)
{
    if (context == 0 || (context->dmCfgToBeValid == 0 && context->dmCfgActValid == 0))
        return 0;
    return context->dmCfgToBeValid != 0 ? context->pDmCfgToBe : context->pDmCfgAct;
}

/* Signal defaults preserve pitches and fields not assigned by the initializer. */
void dovi_dm_InitAlpSigEnv(SigEnvCom_t *signal);
void dovi_dm_InitDmCtrl(DmCtrlFxp_t *control);
int dovi_dm_GetGrcSigEnv(GrcSigEnvFxp_t *output, DmCtxtFxp_t *context);
int dovi_dm_GetSrcSigEnv(SrcSigEnvFxp_t *output, DmCtxtFxp_t *context);
int dovi_dm_GetTgtSigEnv(TgtSigEnvFxp_t *output, DmCtxtFxp_t *context);
int dovi_dm_GetTmCtrl(TmCtrlFxp_t *output, DmCtxtFxp_t *context);
int dovi_dm_GetDmCtrl(DmCtrlFxp_t *output, DmCtxtFxp_t *context);
GrcSigEnvFxp_t *dovi_dm_GetGrcSigEnvPtr(DmCtxtFxp_t *context);
SrcSigEnvFxp_t *dovi_dm_GetSrcSigEnvPtr(DmCtxtFxp_t *context);
TgtSigEnvFxp_t *dovi_dm_GetTgtSigEnvPtr(DmCtxtFxp_t *context);
int dovi_dm_GetStbGmm(unsigned short *minimum_pq, unsigned int *minimum, unsigned short *maximum_pq,
                      unsigned int *maximum, DmCtxtFxp_t *context);
int dovi_dm_SyncSigEnvWithMds(MdsExt_t *metadata, DmCtxtFxp_t *context);
/* Fixed defaults preserve fields not explicitly assigned and accept unaligned backing. */
void dovi_dm_InitSrcSigEnvSec(void *signal);
void dovi_dm_InitSrcSigEnvPri(void *signal);
void dovi_dm_InitTgtSigEnv(void *signal);
void *dovi_dm_InitDmCfg(int platform, void *manager, void *configuration);
#endif
