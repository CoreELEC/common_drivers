/* Display-management commit orchestration. */
#include "dm_bindings.h"
#include "dm_layout.h"
#include "dm_map_workspace.h"

unsigned int dovi_dm_Commit2DmKs(unsigned int signal_index, unsigned int *config,
                                 unsigned int *metadata, unsigned int *context)
{
    _Alignas(16) struct dm_commit_workspace scratch;
    struct dm_user_adjustments *adjustments;
    unsigned int *control;
    unsigned char input_chroma_format;
    unsigned int *signal_flags_or_context;
    _Bool can_bypass_csc;
    unsigned int metadata_change_or_work_value;
    unsigned int commit_result;
    unsigned int mode_rate_or_curve_result;
    unsigned long input_map_result;
    unsigned int *metadata_state_or_curve;
    unsigned int change_flags_or_curve_count;
    unsigned int *mode_state_or_curve_output;
    unsigned long signal_slot;
    unsigned char nonzero_chroma_format;
    long mode_delay_ns;
    short inverse_knot;
    unsigned int previous_mode;
    unsigned int transition_or_frame_duration;
    int compacted_last_index;
    int previous_source_index;
    int inverse_scan_index;
    long kernel_state_address;
    _Bool skip_primary_inverse_rebuild;
    unsigned int *right;

    commit_result = 0xffffffff;

    if ((signal_index == 0) || (context == (unsigned int *)0x0))
        goto return_commit_result;
    if (5 < signal_index)
        goto trap_signal_index;
    kernel_state_address = *(long *)(context + (unsigned long)(signal_index - 1) * 2 + 0x7e46);
    if (config == (unsigned int *)0x0) {
        if (context[1] == 0) {
            _printk(dovi_dm_assertion_format,
                    "../dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmCmmtFxp.c",
                    "UpdateDmCfgSt", 0xbb6, dovi_dm_assertion_false);
            dump_stack();
            goto return_commit_result;
        }
    reuse_saved_configuration:
        config = context + 2;
        *context = 0;
        if (config == (unsigned int *)0x0)
            goto return_commit_result;
    } else {
        if ((context[1] != 0) &&
            (metadata_change_or_work_value = dovi_MemEqByte((const void *)config, (const void *)(context + 2), (int)0x8e4),
             metadata_change_or_work_value != 0))
            goto reuse_saved_configuration;
        *context = 1;
    }
    if (metadata == (unsigned int *)0xffffffffffffffff) {
    reuse_saved_metadata:
        metadata_change_or_work_value = 0;
        commit_result = context[(long)(int)signal_index * 0xb2 + 0x242];
        context[(long)(int)signal_index * 0xb2 + 0x241] = 0;
        right = (unsigned int *)0x0;
        if (commit_result != 0) {
            right = context + (long)(int)signal_index * 0xb2 + 0x243;
        }
    } else {
        if (metadata == (unsigned int *)0x0) {
            mode_state_or_curve_output = context + (long)(int)signal_index * 0xb2;
            metadata_change_or_work_value = (unsigned int)(mode_state_or_curve_output[0x242] != 0);
            right = (unsigned int *)0x0;
        } else {
            if (*metadata == 0)
                goto reuse_saved_metadata;
            if (context[(long)(int)signal_index * 0xb2 + 0x242] == 0) {
                metadata_change_or_work_value = 3;
            } else {
                right = context + (long)(int)signal_index * 0xb2 + 0x243;
                commit_result = dovi_MemEqByte((const void *)metadata, (const void *)right, (int)700);
                if (commit_result != 0) {
                    metadata_change_or_work_value = 0;
                    context[(long)(int)signal_index * 0xb2 + 0x241] = 0;
                    goto merge_metadata_changes;
                }
                commit_result = dovi_MemEqByte((const void *)metadata, (const void *)right, (int)0x78);
                metadata_change_or_work_value = 3;
                if (commit_result != 0) {
                    metadata_change_or_work_value = 6;
                }
            }
            mode_state_or_curve_output = context + (long)(int)signal_index * 0xb2;
            right = metadata;
        }
        mode_state_or_curve_output[0x241] = metadata_change_or_work_value;
    }
merge_metadata_changes:
    if (*context == 0 && (metadata_change_or_work_value & 0xfffffffd) == 0) {
        change_flags_or_curve_count = 0;
    } else {
        change_flags_or_curve_count = *context | 2;
        *context = change_flags_or_curve_count;
    }
    commit_result = metadata_change_or_work_value >> 1 & 2;
    if ((metadata_change_or_work_value & 1) != 0) {
        commit_result = 4;
    }
    if (4 < signal_index) {
    trap_signal_index:
        dm_limit_trap();
    }
    adjustments = (void *)((unsigned char *)context + DM_CONTEXT_USER_ADJUSTMENTS_OFFSET);
    mode_state_or_curve_output = context + 0x10ee;
    adjustments->contrast = ((struct dm_configuration *)config)->tone_control.contrast << 0xe;
    adjustments->saturation_gain = ((struct dm_configuration *)config)->tone_control.saturation_gain
                                   << 0xe;
    adjustments->chroma_weight = ((struct dm_configuration *)config)->tone_control.chroma_weight
                                 << 0xe;
    adjustments->white_point = ((struct dm_configuration *)config)->tone_control.white_point << 0xe;
    adjustments->luminance_gain = ((struct dm_configuration *)config)->tone_control.luminance_gain
                                  << 0xe;
    if (config[0x27] == 0) {
        context[0x7d0f] = 0;
        context[0x7d10] = 0;
        context[0x7d0d] = 0;
        context[0x7d0e] = 0;
        context[0x7d0b] = 0;
        context[0x7d0c] = 0;
        context[0x7d09] = 0;
        context[0x7d0a] = 0;
    } else {
        context[0x7d09] = config[0x28] << (unsigned long)((0x1a - config[0x30]) & 0x1f);
        context[0x7d0a] = config[0x29] << (unsigned long)((0x1a - config[0x30]) & 0x1f);
        context[0x7d0b] = config[0x2a] << (unsigned long)((0x1a - config[0x30]) & 0x1f);
        context[0x7d0c] = config[0x2b] << (unsigned long)((0x1a - config[0x30]) & 0x1f);
        context[0x7d0d] = config[0x2c] << (unsigned long)((0x1a - config[0x30]) & 0x1f);
        context[0x7d0e] = config[0x2d] << (unsigned long)((0x1a - config[0x30]) & 0x1f);
        context[0x7d0f] = config[0x2e] << (unsigned long)((0x1a - config[0x30]) & 0x1f);
        context[0x7d10] = config[0x2f] << (unsigned long)((0x1a - config[0x30]) & 0x1f);
    }
    commit_result = commit_result | (change_flags_or_curve_count & 1);
    metadata_state_or_curve = context + (long)(int)signal_index * 0xb2 + 0x241;
    context[0x7d11] = config[0x3e] << 0xb;
    context[0x7d12] = config[0x3f] << 0xb;
    context[0x7d13] = config[(unsigned long)signal_index * 0x48 + 0x3e] << 0xb;
    metadata_change_or_work_value = config[(unsigned long)signal_index * 0x48 + 0x3f];
    context[0x7cf2] = 0;
    context[0x7d14] = metadata_change_or_work_value << 0xb;
    (context + (unsigned long)signal_index * 0x7de + 0x170a)[0] = 0;
    (context + (unsigned long)signal_index * 0x7de + 0x170a)[1] = 0;
    metadata_change_or_work_value = config[1];
    if (metadata_change_or_work_value != signal_index) {
        if (change_flags_or_curve_count != 0)
            goto commit_configuration_changes;
        if ((*metadata_state_or_curve != 0) || ((*mode_state_or_curve_output != context[0x10ef] && (context[0x10f5] == 0))))
            goto consolidate_selected_input;
        goto publish_metadata_snapshot;
    }
    context[0x10f4] = 0;
    context[0x10f5] = 0;
    if (right == (unsigned int *)0x0) {
        if (context[1] == 0) {
            context[0x10f0] = 1;
        select_disabled_mode:
            mode_rate_or_curve_result = 0;
            goto store_initial_mode;
        }
    clear_previous_mode:
        previous_mode = context[0x10ef];
        mode_rate_or_curve_result = 0;
        transition_or_frame_duration = (unsigned int)(previous_mode != 0);
        context[0x10ef] = 0;
        context[0x10f0] = (unsigned int)(previous_mode != 0);
    } else {
        mode_rate_or_curve_result = config[6];
        transition_or_frame_duration = config[8];
        context[0x10f0] = 0;
        if (context[1] == 0) {
            context[0x10f0] = 1;
            if (transition_or_frame_duration == 1)
                goto select_disabled_mode;
        store_initial_mode:
            transition_or_frame_duration = 1;
            context[0x10ef] = mode_rate_or_curve_result;
        } else {
            if (transition_or_frame_duration == 1)
                goto clear_previous_mode;
            previous_mode = context[0x10ef];
            if ((mode_rate_or_curve_result == 1) && (previous_mode == 0)) {
                transition_or_frame_duration = config[7];
                previous_mode = 0;
                if (transition_or_frame_duration != 0) {
                    if (((*right & 1) == 0) || (*(char *)((long)right + 5) == '\0')) {
                        if ((context[8] == 1) && ((context[9] != 0 && (context[10] != 1)))) {
                            mode_delay_ns = *(long *)(context + 0x10f2) +
                                     (long)(int)(transition_or_frame_duration - context[9]) * 1000000000;
                            *(long *)(context + 0x10f2) = mode_delay_ns;
                            if (4 < config[1])
                                goto trap_signal_index;
                            mode_rate_or_curve_result = config[(unsigned long)config[1] * 0x48 + 0x5e];
                            if ((int)mode_rate_or_curve_result < 2) {
                                mode_rate_or_curve_result = 1;
                            }
                            transition_or_frame_duration = 0;
                            if (mode_rate_or_curve_result != 0) {
                                transition_or_frame_duration = 1000000000 / mode_rate_or_curve_result;
                            }
                            mode_delay_ns = mode_delay_ns - (unsigned long)transition_or_frame_duration;
                        } else {
                            mode_delay_ns = (long)(int)transition_or_frame_duration * 1000000;
                        }
                        *(long *)(context + 0x10f2) = mode_delay_ns;
                        if (mode_delay_ns < 1) {
                            mode_rate_or_curve_result = 1;
                            transition_or_frame_duration = 1;
                            context[0x10f2] = 0;
                            context[0x10f3] = 0;
                            context[0x10ef] = 1;
                            context[0x10f0] = 1;
                        } else {
                            mode_rate_or_curve_result = 0;
                            transition_or_frame_duration = 0;
                        }
                    } else {
                        mode_rate_or_curve_result = 1;
                        transition_or_frame_duration = 1;
                        context[0x10ef] = 1;
                        context[0x10f0] = 1;
                    }
                    goto commit_mode_transition;
                }
            }
            transition_or_frame_duration = (unsigned int)(previous_mode != mode_rate_or_curve_result);
            context[0x10ef] = mode_rate_or_curve_result;
            context[0x10f0] = (unsigned int)(previous_mode != mode_rate_or_curve_result);
        }
    }
commit_mode_transition:
    *mode_state_or_curve_output = mode_rate_or_curve_result;
    commit_result = commit_result | transition_or_frame_duration;
    if (config[2] != 0) {
        context[0x7d40] = context[0x7d40] + 1;
    }
    if (commit_result == 0) {
        if (change_flags_or_curve_count != 0) {
            commit_result = 0;
            goto commit_configuration_changes;
        }
        if (*metadata_state_or_curve == 0) {
            if ((((context[0x7d2b] == 0) || (context + 0x7d2a == (unsigned int *)0x0)) ||
                 (context[0x7d37] != 0)) ||
                ((context[0x7d32] < 2 && (context[0x7d33] < 2)))) {
                context[0x10f4] = 1;
                context[0x10f5] = 1;
                if ((int)*config < 2) {
                    commit_result = 0;
                } else {
                    commit_result = 0;
                    *mode_state_or_curve_output = 0;
                }
                goto publish_metadata_snapshot;
            }
            commit_result = 0x120;
            if (context[0x7d2a] != 0) {
                commit_result = 0x110;
            }
        } else {
            commit_result = 0;
        }
    } else {
    commit_configuration_changes:
        if ((change_flags_or_curve_count >> 1 & 1) != 0) {
            dovi_dm_CommitCfg((unsigned int)signal_index, (long)config, (unsigned char *)context);
        }
    }
consolidate_selected_input:
    signal_flags_or_context = context + (long)(int)signal_index * 0xb2 + 0x240;
    change_flags_or_curve_count = config[(long)(int)signal_index * 0x48 + 0x5f];
    *signal_flags_or_context = change_flags_or_curve_count;
    if (config[2] == 0) {
        change_flags_or_curve_count = change_flags_or_curve_count & 0x7fffffff;
        *signal_flags_or_context = change_flags_or_curve_count;
    }
    signal_slot = (unsigned long)signal_index;
    dovi_dm_ConsolidateInput((unsigned int)signal_index, (int *)config, (unsigned int *)signal_flags_or_context,
                             (unsigned char *)right, (long)context);
    if (((((unsigned char)*metadata_state_or_curve >> 1 & 1) != 0) && ((*signal_flags_or_context & 1) != 0)) &&
        ((right != (unsigned int *)0x0 && ((*right & 1) != 0)))) {
        dovi_dm_CommitMds2KsFrmFmt((unsigned char *)right, (long)(kernel_state_address + 0xbc));
        *(unsigned short *)(kernel_state_address + 0x2c) =
            ~(unsigned short)(-1 << (unsigned long)((unsigned char)right[0x12] & 0x1f));
        nonzero_chroma_format = 1;
        if (*(char *)((long)right + 0x4a) != '\x01') {
            nonzero_chroma_format = 2;
        }
        input_chroma_format = 0;
        if (*(char *)((long)right + 0x4a) != '\0') {
            input_chroma_format = nonzero_chroma_format;
        }
        *(unsigned char *)(kernel_state_address + 0x28) = input_chroma_format;
    }
    if (context[(long)(int)signal_index * 0x7de + 0x1170] == 2) {
        kernel_state_address = *(long *)(context + (unsigned long)(signal_index - 1) * 2 + 0x7e46);
        if ((*mode_state_or_curve_output == context[0x10ef]) || (config[1] != signal_index)) {
            metadata_state_or_curve = context + signal_slot * 0x7de + 0x113a;
            if (right != (unsigned int *)0x0)
                goto select_metadata_bit_depth;
        select_configured_bit_depth:
            metadata_change_or_work_value = config[signal_slot * 0x48 + 0x23];
        } else {
            metadata_state_or_curve = context + 0x113a;
            if (right == (unsigned int *)0x0)
                goto select_configured_bit_depth;
        select_metadata_bit_depth:
            if (((*signal_flags_or_context & 1) == 0) || ((*right & 1) == 0))
                goto select_configured_bit_depth;
            metadata_change_or_work_value = (unsigned int)(unsigned char)right[0x12];
        }
        *(short *)(kernel_state_address + 0x1600) = (short)metadata_change_or_work_value - (short)config[0x23];
        if ((*mode_state_or_curve_output == context[0x10ef]) || (signal_flags_or_context = context, config[1] != signal_index)) {
            signal_flags_or_context = context + signal_slot * 0x7de;
        }
        if ((int)signal_flags_or_context[0x38c6] < 1) {
            metadata_state_or_curve[0x39] = 2;
            if ((*mode_state_or_curve_output == context[0x10ef]) || (mode_state_or_curve_output = context, config[1] != signal_index)) {
                mode_state_or_curve_output = context + signal_slot * 0x7de;
            }
            dovi_CreateToneCurve((void *)metadata_state_or_curve, (void *)(kernel_state_address + 0x540), (unsigned int)0,
                            (void *)(mode_state_or_curve_output + 0x3890), (void *)(context + 0x10e8));
            change_flags_or_curve_count = 0x100;
        } else {
            change_flags_or_curve_count = 0;
        }
        dovi_dm_Commit2Dm3KsIMapBypass((unsigned int)metadata_change_or_work_value, (unsigned char *)(kernel_state_address + 0xd8));
        if (config[1] == signal_index) {
            dovi_dm_Commit2Dm3KsOMapBypass((unsigned int)(config[0x23]),
                                           (long)(*(long *)(kernel_state_address + 0x15f8)));
        }
        commit_result = change_flags_or_curve_count | commit_result;
    } else {
        control = context + (long)(int)signal_index * 0x7de + 0x113a;
        if ((((((unsigned char)*metadata_state_or_curve >> 1 & 1) == 0) || ((*signal_flags_or_context & 1) == 0)) ||
             (right == (unsigned int *)0x0)) ||
            ((*right & 1) == 0)) {
            if ((((unsigned char)*context >> 1 & 1) != 0) &&
                (input_map_result = dovi_dm_CommitSigEnv2KsIMap(
                     (int *)(config + (long)(int)signal_index * 0x48 + 0x19),
                     (long)(context + 0x10e8), (unsigned char *)(kernel_state_address + 0xd8)),
                 (int)input_map_result != 0)) {
                commit_result = 0xfffffffe;
                goto return_commit_result;
            }
        } else {
            dovi_dm_CommitMds2KsIMap((int)signal_index, (unsigned char *)right, (long)control,
                                     (long)(context + 0x10e8), (unsigned char *)(kernel_state_address + 0xd8));
        }
        if ((config[2] == 0) && (context[0x10ef] != 0)) {
            skip_primary_inverse_rebuild = (int)*config < 2;
            if ((metadata_change_or_work_value == signal_index) || ((int)*config < 2))
                goto create_signal_tone_curve;
            if (*mode_state_or_curve_output != 0) {
                _printk(dovi_dm_assertion_format,
                        "../dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmCmmtFxp.c",
                        "Commit2DmKs", 0xd28, "hDm->cmsCtrl.vs10ModeTm == CVs10ModeGpm");
                dump_stack();
            }
            if (((*context != 0) || (*metadata_state_or_curve != 0)) || (context[0x10f5] == 0)) {
                if ((*mode_state_or_curve_output == context[0x10ef]) ||
                    (metadata_state_or_curve = context, config[1] != signal_index)) {
                    metadata_state_or_curve = context + (long)(int)signal_index * 0x7de;
                }
                mode_rate_or_curve_result = dovi_CreateToneCurve(
                    (void *)control, (void *)(context + (long)(int)signal_index * 0x402 + 0x5fe6),
                    (unsigned int)0, (void *)(metadata_state_or_curve + 0x3890), (void *)0x0);
                if ((mode_rate_or_curve_result == 0) && (context[0x10f5] != 0)) {
                    skip_primary_inverse_rebuild = 0;
                    goto commit_primary_output_map;
                }
                dovi_ConcatenatePriBwdMapping(
                    (void *)control, (void *)(context + (long)(int)signal_index * 0x402 + 0x5fe6),
                    (void *)(kernel_state_address + 0x540), (void *)context);
                skip_primary_inverse_rebuild = 0;
                mode_rate_or_curve_result = 0;
                goto merge_tone_curve_changes;
            }
            skip_primary_inverse_rebuild = 0;
        } else {
            skip_primary_inverse_rebuild = 1;
        create_signal_tone_curve:
            if ((*mode_state_or_curve_output == context[0x10ef]) || (metadata_state_or_curve = context, config[1] != signal_index)) {
                metadata_state_or_curve = context + (long)(int)signal_index * 0x7de;
            }
            mode_rate_or_curve_result = dovi_CreateToneCurve((void *)control, (void *)(kernel_state_address + 0x540), (unsigned int)0,
                                    (void *)(metadata_state_or_curve + 0x3890), (void *)0x0);
            if (mode_rate_or_curve_result != 0) {
            merge_tone_curve_changes:
                commit_result = commit_result | (mode_rate_or_curve_result & 0x100000) | 0x100;
            }
        }
    commit_primary_output_map:
        if (metadata_change_or_work_value == signal_index) {
            if ((*context & 2) == 0 && (commit_result & 1) == 0) {
                if ((*mode_state_or_curve_output == context[0x10ef]) ||
                    (metadata_state_or_curve = context, config[1] != signal_index)) {
                    metadata_state_or_curve = context + signal_slot * 0x7de;
                }
                if (context[1] != 0) {
                    can_bypass_csc = dovi_dm_CanCclrBypassCsc((int)(config[0x1f]), (int)(config[0x42]),
                                                     (int)(config[0x24]));
                    if (can_bypass_csc)
                        goto rebuild_primary_inverse_map;
                    can_bypass_csc = dovi_dm_CanCclrBypassCsc((int)(context[0x21]), (int)(context[0x44]),
                                                     (int)(context[0x26]));
                    if (!can_bypass_csc) {
                        if ((*mode_state_or_curve_output == context[0x10ef]) || (config[1] != signal_index)) {
                            if (metadata_state_or_curve[0x1157] == context[signal_slot * 0x7de + 0x38ad]) {
                                signal_flags_or_context = context + signal_slot * 0x7de;
                                goto compare_output_tone_limits;
                            }
                        } else {
                            signal_flags_or_context = context;
                            if (metadata_state_or_curve[0x1157] == context[0x38ad]) {
                            compare_output_tone_limits:
                                if (metadata_state_or_curve[0x1158] == signal_flags_or_context[0x38ae])
                                    goto rebuild_primary_inverse_map;
                            }
                        }
                    }
                }
                dovi_dm_UpdateKsOMapTmm((unsigned int)signal_index, (long)config, (long)context);
            } else {
                dovi_dm_CommitSigEnv2KsOMap((unsigned int)signal_index, (long)config,
                                            (long)context);
            }
        rebuild_primary_inverse_map:
            if (!skip_primary_inverse_rebuild) {
                *mode_state_or_curve_output = 0;
                scratch.flags = change_flags_or_curve_count;
                dovi_dm_ConsolidateInput((unsigned int)signal_index, (int *)config,
                                         (unsigned int *)(&scratch.flags), (unsigned char *)right,
                                         (long)context);
                metadata_change_or_work_value = dovi_CreateToneCurve((void *)(context + 0x113a), (void *)(context + 0x5fe6),
                                        (unsigned int)0, (void *)(context + 0x3890), (void *)0x0);
                context[0x10f5] = (unsigned int)(metadata_change_or_work_value == 0);
                if (metadata_change_or_work_value != 0) {
                    metadata_state_or_curve = context + 0x5fe7;
                    inverse_knot = (short)context[0x5fe6];
                    metadata_change_or_work_value = (unsigned int)inverse_knot;
                    mode_state_or_curve_output = context + 0x75f0;
                    context[0x77f0] = (int)inverse_knot;
                    if (inverse_knot < 1) {
                    store_single_inverse_value:
                        change_flags_or_curve_count = 0;
                        *mode_state_or_curve_output = context[(long)(int)metadata_change_or_work_value + 0x73f0];
                    } else {
                        inverse_knot = (short)*metadata_state_or_curve;
                        inverse_scan_index = 1;
                        compacted_last_index = -1;
                        previous_source_index = -1;
                        do {
                            if (inverse_knot != *(short *)((long)metadata_state_or_curve + (long)inverse_scan_index * 2)) {
                                previous_source_index = inverse_scan_index + -1;
                                compacted_last_index = compacted_last_index + 1;
                                mode_state_or_curve_output[compacted_last_index] = context[(long)previous_source_index + 0x73f0];
                                *(short *)((long)metadata_state_or_curve + (long)compacted_last_index * 2) = inverse_knot;
                                inverse_knot = *(short *)((long)metadata_state_or_curve + (long)inverse_scan_index * 2);
                                metadata_change_or_work_value = context[0x77f0];
                            }
                            inverse_scan_index = inverse_scan_index + 1;
                        } while (inverse_scan_index <= (int)metadata_change_or_work_value);
                        if (compacted_last_index < 0)
                            goto store_single_inverse_value;
                        change_flags_or_curve_count = compacted_last_index + 1;
                        mode_state_or_curve_output[(int)change_flags_or_curve_count] = context[(long)(previous_source_index + 1) + 0x73f0];
                        *(short *)((long)metadata_state_or_curve + (long)(int)change_flags_or_curve_count * 2) = inverse_knot;
                    }
                    context[0x77f0] = change_flags_or_curve_count;
                }
                if ((config[2] == 0) && (config[9] != 0)) {
                    context[0x7d2b] = 1;
                    dovi_dm_ApplyAblu((unsigned int)signal_index, (long)config,
                                      (unsigned int)(scratch.flags), (unsigned char *)right,
                                      (unsigned int *)context);
                }
            }
        }
    }
publish_metadata_snapshot:
    if (metadata == (unsigned int *)0x0) {
        context[(long)(int)signal_index * 0xb2 + 0x242] = 0;
    } else if (right == metadata) {
        context[(long)(int)signal_index * 0xb2 + 0x242] = 1;
        dovi_MemCpyByte((void *)(context + (long)(int)signal_index * 0xb2 + 0x243),
                   (const void *)metadata, (int)700);
    }
return_commit_result:

    return commit_result;
}
