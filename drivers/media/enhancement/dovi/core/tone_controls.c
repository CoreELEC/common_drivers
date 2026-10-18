/* Prepare source and target fields for tone-curve generation. */
#include "tone_controls.h"
#include "dm_bindings.h"
#include "dm_layout.h"
#include "dm_workspace.h"
#include "fixed_access.h"

void dovi_prepare_tone_control(unsigned int signal_index, int *config, unsigned int *flags,
                               unsigned char *metadata, long context, unsigned int *color_state,
                               struct dm_consolidation_workspace *work, unsigned int *input_state,
                               unsigned int *tone_lut_last, int previous_mode, unsigned int metadata_flags)
{
    struct dm_control *control = (void *)config;
    short clamped_mid_pq;
    unsigned short source_pq;
    short clamped_min_pq;
    _Bool continue_update;
    unsigned int tone_word;
    unsigned long source_max_linear;
    unsigned long source_max_conversion;
    unsigned long tone_work;
    unsigned int clamped_tone_value;
    int adjusted_max_pq;
    unsigned int selected_tone_value;
    short *lut_last_pointer;
    unsigned short clamped_max_pq;
    long state_cursor;
    tone_work = signal_index;
    selected_tone_value = (metadata != (unsigned char *)0x0) & metadata_flags;
    if ((selected_tone_value == 1) && ((*metadata & 1) != 0)) {
        source_pq = *(unsigned short *)(metadata + 0x4c);
        *color_state = (unsigned int)source_pq;
        color_state[1] = (unsigned int)*(unsigned short *)(metadata + 0x4e);
        source_max_linear = dovi_Pq2L((unsigned int)((unsigned int)source_pq << 6));
        color_state[2] = (unsigned int)(source_max_linear >> 6);
        source_max_conversion = dovi_Pq2L((unsigned int)(color_state[1] << 6));
        tone_word = color_state[1];
        source_max_linear = source_max_conversion >> 6 & 0xffffffff;
        color_state[3] = (unsigned int)(source_max_conversion >> 6);
    } else {
        tone_word = config[tone_work * 0x48 + 0x58];
        color_state[2] = tone_word;
        color_state[3] = config[tone_work * 0x48 + 0x59];
        tone_word = dovi_LToPQ12((unsigned int)(tone_word));
        *color_state = tone_word & 0xffff;
        tone_word = dovi_LToPQ12((unsigned int)(color_state[3]));
        tone_word = tone_word & 0xffff;
        source_max_linear = (unsigned long)color_state[3];
        color_state[1] = tone_word;
    }
    color_state[6] = color_state[2];
    color_state[7] = (unsigned int)source_max_linear;
    color_state[4] = *color_state;
    color_state[5] = tone_word;
    if ((*(int *)(context + 0x43b8) == *(int *)(context + 0x43bc)) ||
        (control->primary_signal != signal_index)) {
        if ((selected_tone_value == 0) || ((*metadata & 1) == 0)) {
            continue_update = config[tone_work * 0x48 + 0x56] == 3;
            color_state[8] = (unsigned int)continue_update;
            goto check_hlg_source;
        }
        if (*(short *)(metadata + 0x3c) == -2) {
            color_state[8] = 1;
            goto derive_hlg_parameter;
        }
        color_state[8] = 0;
    } else {
        continue_update = config[0x56] == 3;
        color_state[8] = (unsigned int)continue_update;
    check_hlg_source:
        if (continue_update) {
        derive_hlg_parameter:
            tone_work = dovi_sdiv((unsigned long long)(source_max_linear << 9), (unsigned int)(1000));
            tone_word = dovi_fxLOG2Taylor((unsigned int)((unsigned int)tone_work), (unsigned int)(0x1a));
            color_state[9] =
                (int)((unsigned long)((long)(int)((unsigned long)((long)(int)(tone_word + 0x94000000) *
                                                                  0x4d104d42) >>
                                                  0x20) *
                                      0x1ae1) >>
                      0x1a) +
                0x4ccc;
        }
    }
    tone_word = control->primary_signal;
    if (tone_word == signal_index) {
        color_state[0x1d] = config[0x58];
        tone_work = dovi_sdiv((unsigned long long)((unsigned long)(unsigned int)config[0x59] << 0x12),
                      (unsigned int)(0x1900000));
        tone_word = color_state[0x1d];
        if ((unsigned int)tone_work <= color_state[0x1d]) {
            tone_word = (unsigned int)tone_work;
        }
        color_state[0x1d] = tone_word;
        color_state[0x1e] = config[0x59];
        tone_word = dovi_LToPQ12((unsigned int)(tone_word));
        color_state[0x1b] = tone_word & 0xffff;
        tone_word = dovi_LToPQ12((unsigned int)(color_state[0x1e]));
        color_state[0x1c] = tone_word & 0xffff;
        if (*(int *)(context + 0x43b8) == 0) {
            color_state[0x21] = color_state[0x1d];
            color_state[0x22] = color_state[0x1e];
            selected_tone_value = color_state[0x1d] * 100;
            color_state[0x1f] = color_state[0x1b];
            color_state[0x20] = tone_word & 0xffff;
            tone_word = color_state[0x38] * color_state[0x1e] * *(int *)(context + 0x43b0);
            if (selected_tone_value <= tone_word) {
                selected_tone_value = tone_word;
            }
            if (selected_tone_value < 6) {
                selected_tone_value = 5;
            }
            color_state[0x22] = selected_tone_value;
            tone_word = dovi_LToPQ12((unsigned int)(selected_tone_value));
            color_state[0x20] = tone_word & 0xffff;
            selected_tone_value =
                (unsigned int)(((long)*(int *)(context + 0x43b0) * (long)(int)color_state[0x38] >>
                                0x12) *
                                   (unsigned long)color_state[0x1e] >>
                               0x12);
            tone_word = color_state[0x1d] * 100;
            if (color_state[0x1d] * 100 <= selected_tone_value) {
                tone_word = selected_tone_value;
            }
            if (tone_word < 0x140001) {
                tone_word = 0x140000;
            }
            color_state[0x22] = tone_word;
            tone_word = dovi_LToPQ12((unsigned int)(tone_word));
            color_state[0x592] = color_state[0x21];
            color_state[0x593] = color_state[0x22];
            color_state[0x590] = color_state[0x1f];
            color_state[0x20] = tone_word & 0xffff;
            color_state[0x591] = tone_word & 0xffff;
            fixed_store_le64(color_state + 0x58e, fixed_load_le64(color_state + 0x1d));
            fixed_store_le64(color_state + 0x58c, fixed_load_le64(color_state + 0x1b));
            goto check_metadata_backlight;
        }
        fixed_store_le64(color_state + 0x1b, fixed_load_le64(color_state));
        color_state[0x1d] = color_state[2];
        color_state[0x1e] = color_state[3];
        if (metadata != (unsigned char *)0x0)
            goto select_metadata_backlight;
    select_config_backlight:
        *(int *)(context + 0x1f4b8) = config[0x230];
        metadata_flags = config[0x231];
    } else {
        state_cursor = context;
        if (*(int *)(context + 0x43b8) == *(int *)(context + 0x43bc)) {
            if (5 < tone_word) {
                dm_limit_trap();
            }
            state_cursor = context + (unsigned long)tone_word * 0x1f78;
        }
        color_state[0x1d] = *(unsigned int *)(state_cursor + 0xe2b4);
        color_state[0x1e] = *(unsigned int *)(state_cursor + 0xe2b8);
        color_state[0x1b] = *(unsigned int *)(state_cursor + 0xe2ac);
        color_state[0x1c] = *(unsigned int *)(state_cursor + 0xe2b0);
        tone_word = *(unsigned int *)(state_cursor + 0xe2c4);
        color_state[0x21] = tone_word;
        color_state[0x22] = *(unsigned int *)(state_cursor + 0xe2c8);
        selected_tone_value = *(unsigned int *)(state_cursor + 0xe2bc);
        color_state[0x1f] = selected_tone_value;
        color_state[0x20] = *(unsigned int *)(state_cursor + 0xe2c0);
        if (*(int *)(context + 0x43b8) == 1) {
            if ((unsigned int)config[0x58] <= tone_word) {
                tone_word = config[0x58];
            }
            color_state[0x21] = tone_word;
            tone_word = dovi_LToPQ12((unsigned int)(selected_tone_value));
            color_state[0x1f] = tone_word & 0xffff;
            tone_word = color_state[0x22];
            if (color_state[0x22] <= (unsigned int)config[0x59]) {
                tone_word = config[0x59];
            }
            color_state[0x22] = tone_word;
            tone_word = dovi_LToPQ12((unsigned int)(color_state[0x20]));
            color_state[0x20] = tone_word & 0xffff;
        }
    check_metadata_backlight:
        if (metadata == (unsigned char *)0x0)
            goto select_config_backlight;
    select_metadata_backlight:
        if (((metadata_flags >> 4 & 1) == 0) || ((*metadata >> 4 & 1) == 0))
            goto select_config_backlight;
        *(unsigned int *)(context + 0x1f4b8) = (unsigned int)*(unsigned short *)(metadata + 0x2b4);
        metadata_flags = (unsigned int)*(unsigned short *)(metadata + 0x2b6);
    }
    *(unsigned int *)(context + 0x1f4bc) = metadata_flags;
    metadata_flags = *flags;
    if (((metadata == (unsigned char *)0x0) || ((metadata_flags >> 1 & 1) == 0)) ||
        ((*metadata >> 1 & 1) == 0)) {
        if (0x7f < (int)input_state[0x37])
            goto derive_source_pq;
        tone_word = (unsigned int)(unsigned short)*input_state;
        *(short *)((long)input_state + 0x2a) = (short)input_state[1];
    derive_source_midpoint:
        *(short *)(input_state + 10) = (short)tone_word;
        selected_tone_value = tone_word;
        if ((tone_word & 0xffff) < 0x5c4) {
            clamped_tone_value = (tone_word & 0xffff) * 0x19a +
                     (unsigned int)*(unsigned short *)((long)input_state + 0x2a) * 0xe66;
            selected_tone_value = 0x5c3;
            if ((clamped_tone_value & 0xffff000) < 0x5c2001) {
                selected_tone_value = clamped_tone_value >> 0xc;
            }
        }
    } else {
        if (0x7f < (int)input_state[0x37]) {
        derive_source_pq:
            tone_work = (unsigned long)input_state[3] * (long)config[0x1cc] >> 6 & 0x3ffffffffffffc0;
            if ((long)((unsigned long)input_state[3] * (long)config[0x1cc]) < 0x140000000) {
                tone_work = 0x5000000;
            }
            tone_work = dovi_L2PqX((unsigned long long)(tone_work));
            *(short *)((long)input_state + 0x2a) = (short)(tone_work >> 6);
            tone_work = (unsigned long)(unsigned int)config[0x58] << 6;
            if (0x80000 < (unsigned int)config[0x58]) {
                tone_work = 0x2000000;
            }
            tone_work = dovi_L2PqX((unsigned long long)(tone_work));
            tone_word = (unsigned int)(tone_work >> 6) & 0x3ffffff;
            goto derive_source_midpoint;
        }
        *(unsigned short *)((long)input_state + 0x2a) = *(unsigned short *)(metadata + 0x7a);
        tone_word = (unsigned int)*(unsigned short *)(metadata + 0x78);
        *(unsigned short *)(input_state + 10) = *(unsigned short *)(metadata + 0x78);
        selected_tone_value = (unsigned int)*(unsigned short *)(metadata + 0x7c);
    }
    *(short *)(input_state + 0xb) = (short)selected_tone_value;
    if (((metadata != (unsigned char *)0x0) && ((metadata_flags >> 3 & 1) != 0)) &&
        (((*metadata >> 3 & 1) != 0 && (3 < (int)input_state[0x1a])))) {
        adjusted_max_pq = (unsigned int)*(unsigned short *)((long)input_state + 0x2a) +
                 (unsigned int)*(unsigned short *)(metadata + 0x80) * 2 + -0x1000;
        *(unsigned short *)((long)input_state + 0x2a) =
            (unsigned short)adjusted_max_pq & ((unsigned short)(adjusted_max_pq >> 0x1f) ^ 0xffff);
        tone_word =
            ((unsigned int)*(unsigned short *)(metadata + 0x7e) * 2 + (tone_word & 0xffff)) - 0x1000;
        tone_word = tone_word & ((int)tone_word >> 0x1f ^ 0xffffffffU);
        *(short *)(input_state + 10) = (short)tone_word;
        selected_tone_value =
            ((unsigned int)*(unsigned short *)(metadata + 0x82) * 2 + (selected_tone_value & 0xffff)) - 0x1000;
        selected_tone_value = selected_tone_value & ((int)selected_tone_value >> 0x1f ^ 0xffffffffU);
    }
    metadata_flags = input_state[0x1b] & 0xffff;
    *(short *)(input_state + 0x25) = (short)input_state[0x1b];
    *(short *)((long)input_state + 0x96) = (short)input_state[0x1c];
    if ((metadata_flags < 0x5c4) &&
        (metadata_flags = (metadata_flags * 0x19a + (input_state[0x1c] & 0xffff) * 0xe66) >> 0xc, 0x5c2 < metadata_flags)) {
        metadata_flags = 0x5c3;
    }
    clamped_tone_value = selected_tone_value & 0xffff;
    *(short *)(input_state + 0x26) = (short)metadata_flags;
    if (0xffd < clamped_tone_value) {
        clamped_tone_value = 0xffe;
    }
    source_pq = *(unsigned short *)((long)input_state + 0x2a);
    if ((selected_tone_value & 0xffff) == 0) {
        clamped_tone_value = 1;
    }
    clamped_mid_pq = (short)clamped_tone_value;
    clamped_min_pq = (short)tone_word;
    if (clamped_tone_value <= (tone_word & 0xffff)) {
        clamped_min_pq = clamped_mid_pq + -1;
    }
    *(short *)(input_state + 0xb) = clamped_mid_pq;
    clamped_max_pq = source_pq;
    if (0xffe < source_pq) {
        clamped_max_pq = 0xfff;
    }
    if (source_pq <= clamped_tone_value) {
        clamped_max_pq = clamped_mid_pq + 1;
    }
    *(short *)(input_state + 10) = clamped_min_pq;
    *(unsigned short *)((long)input_state + 0x2a) = clamped_max_pq;
    metadata_flags = control->primary_signal;
    if ((control->primary_signal == signal_index) &&
        (metadata_flags = signal_index, *(int *)(context + 0x43bc) == 0 && previous_mode == 0)) {
        dovi_dm_ApplyAblu((unsigned int)(signal_index), (long)((long)config),
                          (unsigned int)(*flags), (unsigned char *)(metadata),
                          (unsigned int *)((unsigned int *)context));
        metadata_flags = control->primary_signal;
    }
    if ((metadata_flags == signal_index) && (previous_mode == 1)) {
        if ((int)input_state[0x37] < 0x80) {
            input_state[0x1f] = (unsigned int)(unsigned short)input_state[10];
            tone_work = dovi_Pq2LX((unsigned int)((unsigned int)(unsigned short)input_state[10]));
            input_state[0x20] = (unsigned int)*(unsigned short *)((long)input_state + 0x2a);
            input_state[0x21] = (unsigned int)tone_work;
            tone_work =
                dovi_Pq2LX((unsigned int)((unsigned int)*(unsigned short *)((long)input_state + 0x2a)));
            clamped_tone_value = (unsigned int)tone_work;
            metadata_flags = input_state[0x20];
            tone_word = input_state[0x21];
            selected_tone_value = input_state[0x1f];
        } else {
            metadata_flags = input_state[1];
            tone_word = input_state[2];
            selected_tone_value = *input_state;
            clamped_tone_value = input_state[3];
            input_state[0x20] = metadata_flags;
            input_state[0x21] = tone_word;
            input_state[0x1f] = selected_tone_value;
        }
        input_state[0x592] = tone_word;
        input_state[0x590] = selected_tone_value;
        input_state[0x591] = metadata_flags;
        input_state[0x22] = clamped_tone_value;
        input_state[0x593] = clamped_tone_value;
        input_state[0x58e] = input_state[0x1d];
        input_state[0x58f] = input_state[0x1e];
        fixed_store_le64(input_state + 0x58c, fixed_load_le64(input_state + 0x1b));
    } else {
        previous_mode = *(int *)(context + 0x43b8);
        if (previous_mode != *(int *)(context + 0x43bc)) {
            if (control->use_metadata != 0) {
                _printk(dovi_dm_assertion_format,
                        "../dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmCmmtFxp.c",
                        "ConsolidateInput", 0xb90, "!pDmCfg->dmCtrl.IsSink");
                dump_stack();
                previous_mode = *(int *)(context + 0x43b8);
            }
            if (previous_mode != 0) {
                _printk(dovi_dm_assertion_format,
                        "../dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmCmmtFxp.c",
                        "ConsolidateInput", 0xb93, "hDm->cmsCtrl.vs10ModeTm == CVs10ModeGpm");
                dump_stack();
            }
            if (*(int *)(context + 0x1f4ac) != 0) {
                input_state[0x1d] = *(unsigned int *)(context + 0x455c);
                input_state[0x1e] = *(unsigned int *)(context + 0x4560);
                input_state[0x1b] = *(unsigned int *)(context + 0x4554);
                input_state[0x1c] = *(unsigned int *)(context + 0x4558);
                input_state[0x21] = *(unsigned int *)(context + 0x456c);
                input_state[0x22] = *(unsigned int *)(context + 0x4570);
                input_state[0x1f] = *(unsigned int *)(context + 0x4564);
                input_state[0x20] = *(unsigned int *)(context + 0x4568);
            }
        }
    }
    if (input_state[0x36] == 1) {
        metadata_flags = 2;
    } else {
        metadata_flags = (unsigned int)(0x7f < (int)input_state[0x37]);
    }
    input_state[0x39] = metadata_flags;
    metadata_flags = control->primary_signal;
    *(unsigned int **)(input_state + 0x6c) = input_state;
    *(unsigned int **)(input_state + 0x6e) = input_state + 0x1b;
    input_state[0x66] = (unsigned int)*(unsigned short *)((long)input_state + 0x2a);
    input_state[0x65] = (unsigned int)(unsigned short)input_state[10];
    input_state[0x67] = (unsigned int)(unsigned short)input_state[0xb];
    if (metadata_flags == signal_index) {
        lut_last_pointer = (short *)(context + 0x17f98);
        metadata_flags = config[0x1c3] - 1;
        if (metadata_flags != (int)*lut_last_pointer) {
            if (0x1fe < (int)metadata_flags) {
                metadata_flags = 0x1ff;
            }
            tone_work = (unsigned long)metadata_flags;
            *tone_lut_last = metadata_flags;
            if (-1 < (int)metadata_flags) {
                state_cursor = 0;
                do {
                    if (state_cursor == 0x200)
                        dm_limit_trap();
                    previous_mode = 0;
                    if ((int)tone_work != 0) {
                        previous_mode = (int)state_cursor / (int)tone_work;
                    }
                    *(int *)(context + 0x1cfc0 + state_cursor * 4) = previous_mode;
                    tone_work = (unsigned long)(int)*tone_lut_last;
                    continue_update = state_cursor < (long)tone_work;
                    state_cursor = state_cursor + 1;
                } while (continue_update);
            }
            if (-1 < *config) {
                previous_mode = 0;
                do {
                    clamped_min_pq = *(short *)tone_lut_last;
                    previous_mode = previous_mode + 1;
                    *lut_last_pointer = clamped_min_pq;
                    lut_last_pointer[0x403] = clamped_min_pq;
                    lut_last_pointer[0x402] = clamped_min_pq;
                    lut_last_pointer[1] = clamped_min_pq;
                    lut_last_pointer = lut_last_pointer + 0x804;
                } while (previous_mode <= *config);
            }
        }
    }
}
