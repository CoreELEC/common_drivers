/* Transfer-function selection and fixed-point de-gamma. */
#include "dm_bindings.h"
#include "dm_layout.h"

int dovi_dm_GetTcLutDir(const void *handle)
{
    const unsigned char *context = handle;
    unsigned int primary = fixed_load_le32(context + DM_CONTEXT_PRIMARY_SIGNAL_OFFSET);
    unsigned int signal_index = primary == 0 ? 1 : primary;
    unsigned int direction;

    if (fixed_load_le32(context + DM_CONTEXT_PREVIOUS_MODE_OFFSET) ==
            fixed_load_le32(context + DM_CONTEXT_CURRENT_MODE_OFFSET) ||
        signal_index != primary) {
        if (signal_index >= DM_SIGNAL_COUNT)
            dm_limit_trap();
        context += (unsigned long)signal_index * DM_CONTEXT_INPUT_STATE_STRIDE;
    }
    direction = fixed_load_le32(context + DM_CONTEXT_TONE_DIRECTION_OFFSET);
    return direction == 0 ? 1 : -(int)(direction == 1);
}

unsigned long dovi_dm_DeGamma(unsigned int gain, unsigned int offset, unsigned int exponent,
                              unsigned int overflow_scale, unsigned int sample)
{
    const unsigned short *log_values = dovi_log10_value;
    const unsigned int *log_rates = (const unsigned int *)dovi_log10_rate;
    const unsigned int (*power_values)[1024] = dovi_power10_value;
    const unsigned char *power_scales = dovi_power10_scale;
#include "dm_degamma_impl.inc"
    return (unsigned long)result;
}

void dovi_dm_GetEotfFromKs(unsigned int signal_index, long context, long signal)
{
    char eotf;
    int direction;
    long signal_context;

    if ((*(int *)(context + 0x43b8) == *(int *)(context + 0x43bc)) ||
        (signal_context = context, *(unsigned int *)(context + 0xc) != signal_index)) {
        if (5 < signal_index)
            goto invalid_signal_index;
        signal_context = context + (unsigned long)signal_index * 0x1f78;
    }
    if (signal_index < 5) {
        if (*(int *)(context + (unsigned long)signal_index * 0x2c8 + 0x908) != 0) {
            eotf = *(char *)(context + (unsigned long)signal_index * 0x2c8 + 0x957);
            direction = (unsigned int)(eotf != '\0') << 1;
            if (eotf == '\x01') {
                direction = 1;
            }
            *(int *)(signal + 0xa4) = direction;
        }
        *(unsigned int *)(signal + 0x100) = *(unsigned int *)(signal_context + 0xe24c);
        *(unsigned int *)(signal + 0xfc) = *(unsigned int *)(signal_context + 0xe248);
        *(unsigned int *)(signal + 0xf4) =
            (unsigned int)(*(char *)(*(long *)(context + (long)(int)signal_index * 8 + 0x1f910) +
                                     0x106) == '\x01');
        return;
    }
invalid_signal_index:
    dm_limit_trap();
}

int dovi_dm_GetTcLutDirIdx(unsigned int signal_index, long context)
{
    if (signal_index == 0) {
        signal_index = 1;
    }
    if ((*(int *)(context + 0x43b8) == *(int *)(context + 0x43bc)) ||
        (signal_index != *(unsigned int *)(context + 0xc))) {
        if (5 < signal_index) {
            dm_limit_trap();
        }
        if (*(int *)(context + (unsigned long)signal_index * 0x1f78 + 0xe324) == 0) {
            return 1;
        }
        if ((*(int *)(context + 0x43b8) == *(int *)(context + 0x43bc)) ||
            (signal_index != *(unsigned int *)(context + 0xc))) {
            context = context + (unsigned long)signal_index * 0x1f78;
        }
    } else if (*(int *)(context + 0xe324) == 0) {
        return 1;
    }
    return -(unsigned int)(*(int *)(context + 0xe324) == 1);
}

int dovi_dm_GetTcLutDirPri(long context)
{
    return dovi_dm_GetTcLutDir((const void *)context);
}
