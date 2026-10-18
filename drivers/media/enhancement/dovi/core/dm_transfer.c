/* Transfer-function selection and fixed-point de-gamma. */
#include "dm_transfer.h"
#include "dm_layout.h"
#include "tone_tables.inc"

int dovi_dm_GetTcLutDir(const void *handle)
{
    const DmCtxtFxp_t *context = handle;
    unsigned int direction = context->tcCtrl.tcLutDir;
    return direction == CTcLutDirFwd ? 1 : -(int)(direction == CTcLutDirBwd);
}

unsigned int dovi_dm_DeGamma(unsigned int gain, unsigned int offset, unsigned int exponent,
                            unsigned int overflow_scale, unsigned int sample)
{
    static const unsigned char scale[10] = {19, 22, 25, 29, 32, 32, 32, 32, 32, 32};
    const unsigned short *log_values = tone_log10_lut[0];
    const unsigned int *log_rates = tone_log10_rate_lut[0];
    const unsigned int (*power_values)[1024] = tone_power10_lut;
    const unsigned char *power_scales = scale;
#include "dm_degamma_impl.inc"
    return (unsigned int)result;
}

int dovi_dm_GetTcLutGDir(const void *handle)
{
    const DmCtxtFxp_t *context = handle;
    const DmCfgFxp_t *config;
    unsigned int direction;
    if (context == 0 || (context->dmCfgToBeValid == 0 && context->dmCfgActValid == 0))
        return 2;
    config = context->dmCfgToBeValid ? context->pDmCfgToBe : context->pDmCfgAct;
    /* The selected pointer is checked, but SecIn is read from active config. */
    if (config == 0 || context->pDmCfgAct->dmCtrl.SecIn == 0)
        return 2;
    direction = context->tc2Ctrl.tcLutDir;
    return direction == CTcLutDirFwd ? 1 : -(int)(direction == CTcLutDirBwd);
}
