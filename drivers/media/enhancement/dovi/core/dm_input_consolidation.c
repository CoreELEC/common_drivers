/* Consolidation of input metadata and color transforms. */
#include "dm_pipeline.h"
#include "dm_configuration.h"
#include "dm_bindings.h"
#include "byte_utils.h"
#include "pq_conversion.h"

void commit_color_matrices(DmCtxtFxp_t *context, DmCfgFxp_t *config)
{
    short (*external_lms_matrix)[3];
    short derived_matrix_scale;
    int external_matrix_scale;

    if ((config->dmCtrl).Lms2IptExt == 0) {
        derived_matrix_scale = dovi_GetLms2IptM33Fxp((context->cscCtrl).m33Lms2Ipt);
        (context->cscCtrl).m33Lms2IptScale2P = (int)derived_matrix_scale;
        if ((config->dmCtrl).Ipt2LmsExt == 0) {
            derived_matrix_scale = dovi_GetIpt2LmsM33Fxp((context->cscCtrl).m33Ipt2Lms);
            (context->cscCtrl).m33Ipt2LmsScale2P = (int)derived_matrix_scale;
            goto check_external_ipt_to_lms;
        }
    copy_external_ipt_to_lms:
        dovi_MemCpyByte((context->cscCtrl).m33Ipt2Lms, (config->dmCtrl).M33Ipt2Lms, 0x12);
        (context->cscCtrl).m33Ipt2LmsScale2P = (config->dmCtrl).M33Lms2IptScale2P;
    } else {

        external_lms_matrix = (config->dmCtrl).M33Lms2Ipt;
        dovi_MemCpyByte((context->cscCtrl).m33Lms2Ipt, external_lms_matrix, 0x12);
        external_matrix_scale = (config->dmCtrl).M33Lms2IptScale2P;
        (context->cscCtrl).m33Lms2IptScale2P = external_matrix_scale;
        (context->cscCtrl).m33Ipt2LmsScale2P = external_matrix_scale;
        if ((config->dmCtrl).Ipt2LmsExt == 0) {
            derived_matrix_scale = dovi_InvM33(external_lms_matrix, (context->cscCtrl).m33Ipt2Lms, (short)external_matrix_scale, (short)external_matrix_scale);
            (context->cscCtrl).m33Ipt2LmsScale2P = fixed_signed32(
                (unsigned int)(context->cscCtrl).m33Ipt2LmsScale2P - (unsigned int)(int)derived_matrix_scale);
        check_external_ipt_to_lms:
            if ((config->dmCtrl).Ipt2LmsExt != 0)
                goto copy_external_ipt_to_lms;
        } else {
            dovi_MemCpyByte((context->cscCtrl).m33Ipt2Lms, (config->dmCtrl).M33Ipt2Lms, 0x12);
            (context->cscCtrl).m33Ipt2LmsScale2P = (config->dmCtrl).M33Lms2IptScale2P;
        }
    }
}

unsigned int commit_configuration_tone_maps(DmCtxtFxp_t *context, DmCfgFxp_t *config,
                                            DmKsFxp_t *kernel)
{
    TcCtrlFxp_t *video_curve;
    TcCtrlFxp_t *graphics_curve;
    TgtSigEnvFxp_t *target_signal;
    unsigned int graphics_minimum;
    unsigned int minimum_limit_or_margin;
    unsigned int maximum_limit;
    int source_pq_bounds;
    unsigned short graphics_source_max_pq;
    unsigned short graphics_source_min_pq;
    unsigned short pq_endpoint_a;
    unsigned short pq_endpoint_b;
    unsigned short signal_diagonal;
    unsigned short source_pq_value;
    unsigned int tone_control_word;
    unsigned int source_linear_maximum;
    unsigned int tone_limit_or_flags;
    unsigned int tone_direction;
    int signal_is_hlg;
    unsigned int signal_limit;
    unsigned int video_limit_or_flags;

    video_curve = &context->tcCtrl;
    graphics_curve = &context->tc2Ctrl;
    target_signal = &config->tgtSigEnv;
    target_signal_to_tc_ctrl(target_signal, video_curve);
    pq_endpoint_a = (context->tcCtrl).tcTgtSig.minPq;
    video_limit_or_flags = (unsigned int)pq_endpoint_a;
    pq_endpoint_b = (context->tcCtrl).tcTgtSig.maxPq;
    tone_control_word = (unsigned int)pq_endpoint_b;
    (context->tcCtrl).tcTgtSig.crush = pq_endpoint_a;
    (context->tcCtrl).tcTgtSig.clip = pq_endpoint_b;
    (context->tcCtrl).tcTgtSig.mid =
        (unsigned short)((int)((unsigned int)pq_endpoint_a + (unsigned int)pq_endpoint_b) >> 1);
    if ((config->dmCtrl).MdsFollowed == 0) {
        source_pq_bounds = dm_pair((config->srcSigEnv).MinPq, (config->srcSigEnv).MaxPq);
        if (source_pq_bounds == 0) {
            signal_diagonal = (config->srcSigEnv).DiagSize;
            source_linear_maximum = (config->srcSigEnv).Max;
            source_pq_value = dovi_LToPQ12((config->srcSigEnv).Min);
            signal_limit = (unsigned int)source_pq_value;
            (context->tcCtrl).tcSrcSig.minPq = source_pq_value;
            source_pq_value = dovi_LToPQ12(source_linear_maximum);
            (context->tcCtrl).tcSrcSig.maxPq = source_pq_value;
            tone_limit_or_flags = (unsigned int)source_pq_value;
            (context->tcCtrl).tcSrcSig.diagSize = signal_diagonal;
            signal_is_hlg = (config->srcSigEnv).Eotf == CEotfHlg;
            (context->tcCtrl).tcSrcSig.hlgA = (unsigned short)signal_is_hlg;
            if (signal_is_hlg) {
                update_tc_signal_hlg_a((config->srcSigEnv).Min, (config->srcSigEnv).Max,
                                &video_curve->tcSrcSig);
                video_limit_or_flags = (unsigned int)(context->tcCtrl).tcTgtSig.minPq;
                signal_limit = (unsigned int)(context->tcCtrl).tcSrcSig.minPq;
                tone_limit_or_flags = (unsigned int)(context->tcCtrl).tcSrcSig.maxPq;
                tone_control_word = (unsigned int)(context->tcCtrl).tcTgtSig.maxPq;
            }
        } else {
            pq_endpoint_a = (config->srcSigEnv).MinPq;
            signal_limit = (unsigned int)pq_endpoint_a;
            (context->tcCtrl).tcSrcSig.minPq = pq_endpoint_a;
            pq_endpoint_a = (config->srcSigEnv).MaxPq;
            tone_limit_or_flags = (unsigned int)pq_endpoint_a;
            (context->tcCtrl).tcSrcSig.maxPq = pq_endpoint_a;
            (context->tcCtrl).tcSrcSig.diagSize = (config->srcSigEnv).DiagSize;
        }
        (context->tcCtrl).tcSrcSig.crush = (unsigned short)signal_limit;
        tone_direction = CTcLutDirBwd;
        (context->tcCtrl).tcSrcSig.clip = (unsigned short)tone_limit_or_flags;
        (context->tcCtrl).tcSrcSig.mid = (unsigned short)((int)(signal_limit + tone_limit_or_flags) >> 1);
        if (((tone_control_word <= tone_limit_or_flags + 1) && (tone_direction = CTcLutDirFwd, tone_limit_or_flags <= tone_control_word + 1)) &&
            ((signal_limit + 1 < video_limit_or_flags || (tone_direction = CTcLutDirPass, video_limit_or_flags + 1 < signal_limit)))) {
            tone_direction = (unsigned int)(tone_limit_or_flags < tone_control_word);
        }
        (context->tcCtrl).tcLutDir = tone_direction;
        tm_ctrl_to_tc_ctrl(&config->tmCtrl, (DmCtrlFxp_t *)&(config->dmCtrl).StbMode, video_curve, 0);
        update_tc_ctrl_level(&config->tmCtrl, video_curve);
        tone_control_word = (context->tcCtrl).level2[2];
        if (0 < (int)tone_control_word) {
            tone_control_word = 0;
        }
        (context->tcCtrl).level2[2] = tone_control_word;
        tone_control_word = dovi_CreateToneCurve(video_curve, &kernel->ksTMap, &context->tcCtrlUsed);
        if (tone_control_word == 0)
            goto configuration_changed;
        video_limit_or_flags = 0x11;
        if ((tone_control_word >> 0x14 & 1) != 0) {
            video_limit_or_flags = 0x100011;
        }
    } else {
    configuration_changed:
        video_limit_or_flags = 1;
    }

    signal_is_hlg = (config->grcSigEnv).Eotf == CEotfHlg;
    (context->tc2Ctrl).tcSrcSig.hlgA = (unsigned short)signal_is_hlg;
    if ((config->dmCtrl).StbMode == CStbModeVpri) {
        tone_control_word = (config->dmCtrl).PMin;
        tone_limit_or_flags = (config->dmCtrl).PMax;
    } else {
        tone_limit_or_flags = (config->grcSigEnv).Max;
        signal_limit = (config->tgtSigEnv).Max;
        if (tone_limit_or_flags < signal_limit) {
            graphics_minimum = (config->grcSigEnv).Min;
            minimum_limit_or_margin = (config->dmCtrl).PMin;
            maximum_limit = (config->dmCtrl).PMax;
            tone_control_word = (config->tgtSigEnv).Min;
            if (graphics_minimum < minimum_limit_or_margin) {
                minimum_limit_or_margin = graphics_minimum;
            }
            if (maximum_limit < tone_limit_or_flags) {
                maximum_limit = tone_limit_or_flags;
            }
            if (tone_control_word < minimum_limit_or_margin) {
                tone_control_word = minimum_limit_or_margin;
            }
            tone_limit_or_flags = maximum_limit;
            if (signal_limit < maximum_limit) {
                tone_limit_or_flags = signal_limit;
            }
        } else {
            tone_control_word = (config->grcSigEnv).Min;
        }
    }
    signal_diagonal = (config->grcSigEnv).DiagSize;
    pq_endpoint_a = dovi_LToPQ12(tone_control_word);
    (context->tc2Ctrl).tcSrcSig.minPq = pq_endpoint_a;
    pq_endpoint_b = dovi_LToPQ12(tone_limit_or_flags);
    (context->tc2Ctrl).tcSrcSig.diagSize = signal_diagonal;
    (context->tc2Ctrl).tcSrcSig.maxPq = pq_endpoint_b;
    if (signal_is_hlg) {
        update_tc_signal_hlg_a((config->grcSigEnv).Min, (config->grcSigEnv).Max,
                        &graphics_curve->tcSrcSig);
        pq_endpoint_a = (context->tc2Ctrl).tcSrcSig.minPq;
        pq_endpoint_b = (context->tc2Ctrl).tcSrcSig.maxPq;
    }
    (context->tc2Ctrl).tcSrcSig.crush = pq_endpoint_a;
    (context->tc2Ctrl).tcSrcSig.clip = pq_endpoint_b;
    (context->tc2Ctrl).tcSrcSig.mid =
        (unsigned short)((int)((unsigned int)pq_endpoint_a + (unsigned int)pq_endpoint_b) >> 1);
    tone_limit_or_flags = video_limit_or_flags;
    if (((config->dmCtrl).MdsFollowed == 0) || ((config->dmCtrl).StbMode == CStbModeGpri)) {
        target_signal_to_tc_ctrl(target_signal, graphics_curve);
        pq_endpoint_a = (context->tc2Ctrl).tcTgtSig.maxPq;
        tone_direction = CTcLutDirBwd;
        pq_endpoint_b = (context->tc2Ctrl).tcTgtSig.minPq;
        graphics_source_max_pq = (context->tc2Ctrl).tcSrcSig.maxPq;
        (context->tc2Ctrl).tcTgtSig.crush = pq_endpoint_b;
        (context->tc2Ctrl).tcTgtSig.mid =
            (unsigned short)((int)((unsigned int)pq_endpoint_b + (unsigned int)pq_endpoint_a) >> 1);
        (context->tc2Ctrl).tcTgtSig.clip = pq_endpoint_a;
        tone_control_word = (unsigned int)pq_endpoint_a;
        graphics_source_min_pq = (context->tc2Ctrl).tcSrcSig.minPq;
        if (tone_control_word <= graphics_source_max_pq + 1u) {
            tone_direction = CTcLutDirFwd;
            signal_limit = (unsigned int)graphics_source_max_pq;
            if (signal_limit == tone_control_word + 1 || (unsigned int)graphics_source_max_pq < tone_control_word + 1) {
                if ((graphics_source_min_pq + 1u < (unsigned int)pq_endpoint_b) ||
                    (minimum_limit_or_margin = pq_endpoint_b + 1, tone_direction = CTcLutDirPass,
                     graphics_source_min_pq != minimum_limit_or_margin && minimum_limit_or_margin <= graphics_source_min_pq)) {
                    tone_direction = (unsigned int)(signal_limit <= tone_control_word && pq_endpoint_a != signal_limit);
                }
            }
        }
        (context->tc2Ctrl).tcLutDir = tone_direction;
        tm_ctrl_to_tc_ctrl(&config->tmCtrl, (DmCtrlFxp_t *)&(config->dmCtrl).StbMode, graphics_curve, 1);
        update_tc_ctrl_level(&config->tmCtrl, graphics_curve);
        tone_control_word = dovi_CreateToneCurve(graphics_curve, &kernel->ksTMapG, &context->tc2CtrlUsed);
        if (tone_control_word != 0) {
            tone_limit_or_flags = video_limit_or_flags | 0x200020;
            if ((tone_control_word & 0x100000) == 0) {
                tone_limit_or_flags = video_limit_or_flags | 0x20;
            }
        }
    }
    tone_control_word = dovi_dm_CommitSigEnv2KsOMap(config, 1, context);
    tone_control_word = tone_control_word | tone_limit_or_flags;
    dovi_dm_CommitCdm2KsFrmFmt((SigEnvCom_t *)target_signal, &kernel->ksFrmFmtO);

    return tone_control_word;
}

/* Synchronize the active source environment from metadata, in write order. */
int dovi_dm_SyncSigEnvWithMds(MdsExt_t *metadata, DmCtxtFxp_t *context)
{
    DmCfgFxp_t *configuration;
    unsigned int color, chroma, range, eotf;
    if (context == 0 || context->dmCfgActValid == 0)
        return -1;
    configuration = context->pDmCfgAct;
    color = metadata->signal_color_space == 0   ? CClrYuv
            : metadata->signal_color_space == 1 ? CClrRgb
                                                : CClrIpt;
    configuration->srcSigEnv.Clr = color;
    chroma = metadata->signal_chroma_format == 0   ? CChrm420
             : metadata->signal_chroma_format == 1 ? CChrm422
                                                   : CChrm444;
    configuration->srcSigEnv.Chrm = chroma;
    chroma = metadata->signal_chroma_format;
    configuration->srcSigEnv.Dtp = CDtpU16;
    configuration->srcSigEnv.Weav = chroma == 1 ? CWeavUyVy : CWeavPlnr;
    configuration->srcSigEnv.Bdp = metadata->signal_bit_depth;
    range = metadata->signal_full_range_flag;
    if (range != 0 && range != 1)
        range = CRngSdi;
    configuration->srcSigEnv.Rng = range;
    if (metadata->signal_eotf == 0xffff)
        eotf = CEotfPq;
    else if (metadata->signal_eotf == 0xfffe)
        eotf = CEotfHlg;
    else
        eotf = metadata->signal_eotf == 0xfffd ? CEotfPower : CEotfBt1886;
    configuration->srcSigEnv.Eotf = eotf;
    configuration->srcSigEnv.Min = dovi_PQ12ToL(metadata->source_min_PQ);
    configuration->srcSigEnv.Max = dovi_PQ12ToL(metadata->source_max_PQ);
    return 0;
}
