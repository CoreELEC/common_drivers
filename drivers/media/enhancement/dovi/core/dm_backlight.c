/* Adaptive backlight updates and global dimming results. */
#include "dm_bindings.h"
#include "dm_layout.h"
#include "fixed_access.h"

void dovi_dm_ApplyAblu(unsigned int signal_index, long config, unsigned int flags,
                       unsigned char *metadata, unsigned int *context)
{
    int anchor_correction;
    unsigned int *backlight_state;
    int anchor_delta;
    unsigned int *signal_state;
    unsigned int filtered_value;
    unsigned int update_word;
    unsigned int output_word;
    unsigned long linear_limit;
    unsigned long filtered_linear;
    long filter_work;

    if (context[0x10ee] == context[0x10ef]) {
        update_word = *(unsigned int *)(config + 4);
        if (5 < update_word) {
            dm_limit_trap();
        }
        signal_state = context + (unsigned long)update_word * 0x7de;
    } else {
        update_word = *(unsigned int *)(config + 4);
        signal_state = context;
    }
    output_word = *context;
    backlight_state = context + 0x7d2a;
    if (update_word == signal_index) {
        update_word = context[0x7d2b];
    } else {
        _printk(dovi_dm_assertion_format,
                "../dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmCmmtFxp.c",
                "ApplyAblu", 0x6fb, "sigEnvIdx == pDmCfg->dmCtrl.PriInputIdx");
        dump_stack();
        update_word = context[0x7d2b];
    }
    if ((update_word == 0) || (backlight_state == (unsigned int *)0x0)) {
        if ((short)context[0x7d34] == 0) {
            return;
        }
        signal_state[0x1157] = signal_state[0x16c8];
        signal_state[0x1158] = signal_state[0x16c9];
        fixed_store_le64(signal_state + 0x1155, fixed_load_le64(signal_state + 0x16c6));
        signal_state[0x115b] = signal_state[0x16cc];
        signal_state[0x115c] = signal_state[0x16cd];
        fixed_store_le64(signal_state + 0x1159, fixed_load_le64(signal_state + 0x16ca));
        *(unsigned short *)(context + 0x7d34) = 0;
        return;
    }
    if ((((flags >> 4 & 1) == 0) || (metadata == (unsigned char *)0x0)) ||
        ((*metadata >> 4 & 1) == 0)) {
        if (*backlight_state == 0) {
            context[0x7d2e] = signal_state[0x16c7];
            context[0x7d2f] = 0;
            goto mark_backlight_active;
        }
        filtered_value = 0x1000;
        update_word = 0x1000;
        context[0x7d2e] = 0x1000;
        context[0x7d2f] = 0x1000;
    } else {
        if (*backlight_state == 0)
            goto mark_backlight_active;
        update_word = context[0x7d2e];
        filtered_value = context[0x7d2f];
    }
    anchor_delta = *(unsigned short *)((long)signal_state + 0x4512) - update_word;
    anchor_correction = (anchor_delta * 0x19d >> 0xc) + 0xf9;
    if (anchor_delta < 0) {
        anchor_correction = 0xf9;
    }
    update_word = anchor_correction + ((update_word * context[0x7d38] + filtered_value * context[0x7d39]) >> 0xc);
    filtered_value = dovi_PQ12ToL((unsigned int)(update_word));
    context[0x7d42] = filtered_value;
    context[0x7d43] = update_word & 0xffff;
    if (context[0x7d2c] == 0) {
        filtered_linear = (unsigned long)context[0x7d36];
        context[0x7d37] = 0;
        update_word = context[0x7d2d];
        if (context[0x7d36] != update_word) {
            filtered_linear = (unsigned long)update_word;
            context[0x7d35] = update_word;
            context[0x7d36] = update_word;
        }
    } else {
        linear_limit = (long)((unsigned long)context[0x7d3b] * (unsigned long)filtered_value) >> 0x12;
        filtered_linear = (long)(int)context[0x10ec] * (unsigned long)context[0x7d30] >> 0x12 & 0xffffffff;
        if (linear_limit <= filtered_linear) {
            filtered_linear = linear_limit;
        }
        linear_limit = dovi_sdiv((unsigned long long)(0x5000000000), (unsigned int)(signal_state[0x1172]));
        update_word = (unsigned int)linear_limit;
        if ((unsigned int)linear_limit <= context[0x7d31]) {
            update_word = context[0x7d31];
        }
        if (update_word <= filtered_linear) {
            update_word = (unsigned int)filtered_linear;
        }
        filtered_linear = (unsigned long)update_word;
        context[0x7d35] = update_word;
        if ((short)context[0x7d34] != 0) {
            filter_work = 0x20;
            if (update_word <= context[0x7d36]) {
                filter_work = 0x24;
            }
            filtered_linear =
                ((unsigned long)*(unsigned int *)((long)backlight_state + filter_work) * (unsigned long)update_word +
                 (unsigned long)context[0x7d36] *
                     (unsigned long)(0x1000 - *(unsigned int *)((long)backlight_state + filter_work))) >>
                0xc;
        }
        context[0x7d36] = (unsigned int)filtered_linear;
        context[0x7d37] = 0;
    }
    update_word = (unsigned int)filtered_linear;
    if ((((output_word >> 1 & 1) == 0) && ((short)context[0x7d34] != 0)) && (*backlight_state != 0)) {
        filter_work = (unsigned long)context[0x7d3d] - (filtered_linear & 0xffffffff);
        if ((((filter_work <= (long)-(unsigned long)context[0x7d41]) ||
              ((long)(unsigned long)context[0x7d41] <= filter_work)) &&
             (context[0x7d3f] <= context[0x7d40])) ||
            ((update_word != context[0x7d3d] &&
              (((context[0x7d3f] << 1 <= context[0x7d40] && 1 < context[0x7d3f]) ||
                (0x3b < context[0x7d40]))))))
            goto publish_target_luminance;
    } else {
    publish_target_luminance:
        signal_state[0x1158] = update_word;
        update_word = dovi_LToPQ12((unsigned int)(update_word));
        signal_state[0x1156] = update_word & 0xffff;
        update_word = (unsigned int)((long)(int)signal_state[0x1172] * (unsigned long)signal_state[0x1158] >> 0x12);
        signal_state[0x115c] = update_word;
        update_word = dovi_LToPQ12((unsigned int)(update_word));
        signal_state[0x115a] = update_word & 0xffff;
    }
    update_word = context[0x7d3a];
    context[0x7d3d] = signal_state[0x1158];
    if (update_word == 0) {
        output_word = 0x1900000;
    } else {
        output_word = 0;
        if ((unsigned long)update_word != 0) {
            output_word = (unsigned int)(((unsigned long)context[0x7d36] << 0x12) / (unsigned long)update_word);
        }
    }
    context[0x7d3e] = output_word;
    signal_state[0x1157] = output_word;
    update_word = dovi_LToPQ12((unsigned int)(output_word));
    signal_state[0x1155] = update_word & 0xffff;
    update_word = (unsigned int)((long)(int)signal_state[0x1172] * (unsigned long)signal_state[0x1157] >> 0x12);
    signal_state[0x115b] = update_word;
    update_word = dovi_LToPQ12((unsigned int)(update_word));
    signal_state[0x1159] = update_word & 0xffff;
mark_backlight_active:
    *(unsigned short *)(context + 0x7d34) = 1;
    return;
}

unsigned long dovi_dm_GetGdActiveFltrdAnchor(unsigned short *filtered_pq,
                                             unsigned int *filtered_linear, long context)
{
    const struct dm_global_dimming_result *result =
        (const void *)(context + DM_CONTEXT_GD_RESULT_OFFSET);
    if ((context == 0) || (result->active == 0)) {
        return 0xffffffff;
    }
    if (filtered_linear != (unsigned int *)0x0) {
        *filtered_linear = result->filtered_linear;
    }
    if (filtered_pq != (unsigned short *)0x0) {
        *filtered_pq = (short)result->filtered_pq;
        return 0;
    }
    return 0;
}

unsigned long dovi_dm_GetGdActiveFltrdMean(unsigned short *filtered_pq,
                                           unsigned int *filtered_linear, long context)
{
    const struct dm_global_dimming_result *result =
        (const void *)(context + DM_CONTEXT_GD_RESULT_OFFSET);
    if ((context == 0) || (result->active == 0)) {
        return 0xffffffff;
    }
    if (filtered_linear != (unsigned int *)0x0) {
        *filtered_linear = result->filtered_linear;
    }
    if (filtered_pq != (unsigned short *)0x0) {
        *filtered_pq = (short)result->filtered_pq;
        return 0;
    }
    return 0;
}

unsigned long dovi_dm_GetGdActiveTgtWindow(unsigned int *window_word_28,
                                           unsigned int *window_word_24, long context)
{
    const struct dm_global_dimming_result *result =
        (const void *)(context + DM_CONTEXT_GD_RESULT_OFFSET);
    if ((context == 0) || (result->active == 0)) {
        return 0xffffffff;
    }
    if (window_word_28 != (unsigned int *)0x0) {
        *window_word_28 = result->window_word_28;
    }
    if (window_word_24 != (unsigned int *)0x0) {
        *window_word_24 = result->window_word_24;
        return 0;
    }
    return 0;
}

unsigned long dovi_dm_GetGdBackLightModelInput(unsigned int *output, long context)
{
    const struct dm_global_dimming_result *result =
        (const void *)(context + DM_CONTEXT_GD_RESULT_OFFSET);
    if ((context != 0) && (result->active != 0)) {
        if (output != (unsigned int *)0x0) {
            *output = result->model_input;
            return 0;
        }
        return 0;
    }
    return 0xffffffff;
}

unsigned long dovi_dm_GetGdBackLightModelOutput(unsigned int *output, long context)
{
    const struct dm_global_dimming_result *result =
        (const void *)(context + DM_CONTEXT_GD_RESULT_OFFSET);
    if ((context != 0) && (result->active != 0)) {
        if (output != (unsigned int *)0x0) {
            *output = result->model_output;
            return 0;
        }
        return 0;
    }
    return 0xffffffff;
}
