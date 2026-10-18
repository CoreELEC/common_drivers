/* Input color-space and transfer maps. */
#include "dm_pipeline.h"
#include "fixed_bits.h"
#include "color_basis.h"
#include "matrix_arithmetic.h"
#include "color_narrow.h"
#include "color_primaries.h"
#include "numeric_color_matrix.h"
#include "numeric_helpers.h"
#include "dm_bindings.h"
#include "byte_utils.h"
#include "pq_conversion.h"
#include "dm_commit_tables.inc"

void dovi_dm_CommitMdsCsc(MdsExt_t *hMdsExt, DmCfgFxp_t *pDmCfg, CscCtrlFxp_t *pCscCtrl,
                          DmKsIMapFxp_t *pKsIMap)

{
    short (*next_yuv_row)[3];
    long long next_lut_byte_offset;
    int crossTalk;
    int lms_to_ipt_scale;
    unsigned char full_range_flag;
    short m33Scale;
    short rgb_to_lms_scale;
    unsigned int range_or_gamma_value;
    unsigned int rng;
    short (*source_matrix_row)[3];
    long long range_or_lut_offset;
    short (*matrix_row_work)[3];
    CscCtrlFxp_t *rgb_matrix_target;
    unsigned short eotf_or_sample;
    unsigned int *gamma_lut_cursor;
    unsigned int *next_gamma_lut_word;
    int v3[3];
    short m33Lms2Ipt[3][3];
    short m33Ipt2Lms[3][3];
    crossTalk = (pDmCfg->srcSigEnv).CrossTalk;
    dovi_MemCpyByte(m33Ipt2Lms, pCscCtrl->m33Ipt2Lms, 0x12);
    dovi_MemCpyByte(m33Lms2Ipt, pCscCtrl->m33Lms2Ipt, 0x12);
    lms_to_ipt_scale = pCscCtrl->m33Lms2IptScale2P;
    pKsIMap->clr = hMdsExt->signal_color_space == '\x01';
    if (hMdsExt->signal_color_space == '\x02') {
        pKsIMap->clr = '\0';
    }
    pKsIMap->m33Yuv2RgbScale2P = hMdsExt->m33Yuv2RgbScale2P;
    source_matrix_row = hMdsExt->m33Yuv2Rgb;
    matrix_row_work = pKsIMap->m33Yuv2Rgb;
    do {
        next_yuv_row = source_matrix_row + 1;
        (*matrix_row_work)[0] = (*source_matrix_row)[0];
        (*matrix_row_work)[1] = (*source_matrix_row)[1];
        (*matrix_row_work)[2] = (*source_matrix_row)[2];
        source_matrix_row = next_yuv_row;
        matrix_row_work = matrix_row_work + 1;
    } while (next_yuv_row != (short (*)[3])((unsigned char *)hMdsExt + 22));
    dovi_Yuv2RgbOffsetRgbFxp(pKsIMap->m33Yuv2Rgb, hMdsExt->v3Yuv2Rgb[0], hMdsExt->v3Yuv2Rgb[1],
                        pKsIMap->v3Yuv2RgbOffInRgb);
    m33Scale = hMdsExt->m33Rgb2WpLmsScale2P;
    pCscCtrl->sM33Rgb2LmsScale2P = (int)m33Scale;
    source_matrix_row = hMdsExt->m33Rgb2WpLms;
    rgb_matrix_target = pCscCtrl;
    do {
        matrix_row_work = source_matrix_row + 1;
        rgb_matrix_target->sM33Rgb2Lms[0][0] = (*source_matrix_row)[0];
        rgb_matrix_target->sM33Rgb2Lms[0][1] = (*source_matrix_row)[1];
        rgb_matrix_target->sM33Rgb2Lms[0][2] = (*source_matrix_row)[2];
        source_matrix_row = matrix_row_work;
        rgb_matrix_target = (CscCtrlFxp_t *)(rgb_matrix_target->sM33Rgb2Lms + 1);
    } while (matrix_row_work != (short (*)[3]) & hMdsExt->signal_eotf);
    if (hMdsExt->signal_color_space == '\x02') {
        v3[0] = 0x7fff;
        v3[1] = 0x7fff;
        v3[2] = 0x7fff;
        rgb_to_lms_scale = dovi_GetRgb2LmsCtWpM33Fxp(pCscCtrl, m33Scale, crossTalk, v3, 0xf, pKsIMap->m33Rgb2Lms);
        pKsIMap->m33Rgb2LmsScale2P = rgb_to_lms_scale;
    } else {
        rgb_to_lms_scale = dovi_GetRgb2LmsCtWpM33Fxp(pCscCtrl, m33Scale, crossTalk, pCscCtrl->sV3Wp,
                                     (short)pCscCtrl->sV3WpScale2P, pKsIMap->m33Rgb2Lms);
        pKsIMap->m33Rgb2LmsScale2P = rgb_to_lms_scale;
    }
    full_range_flag = hMdsExt->signal_full_range_flag;
    rng = (unsigned int)full_range_flag;
    if ((full_range_flag != 1) && (rng = CRngNarrow, full_range_flag != 0)) {
        rng = CRngSdi;
    }
    dovi_GetRange((unsigned int)hMdsExt->signal_bit_depth, rng, &(pKsIMap->eotfParam).rangeMin,
             &(pKsIMap->eotfParam).range);
    if (hMdsExt->signal_bit_depth == 0) {
        (pKsIMap->eotfParam).rangeInv = 0x10000;
        (pKsIMap->eotfParam).bdp = (unsigned short)hMdsExt->signal_bit_depth;
        eotf_or_sample = hMdsExt->signal_eotf;
    } else {

        range_or_lut_offset =
            (long long)(int)((unsigned int)(pKsIMap->eotfParam).range
                             << (unsigned long long)((0x10 - hMdsExt->signal_bit_depth) & 0x1f));
        range_or_gamma_value = 0;
        if (range_or_lut_offset != 0) {
            range_or_gamma_value = (unsigned int)(0xffffffff / range_or_lut_offset);
        }
        (pKsIMap->eotfParam).rangeInv = range_or_gamma_value;
        (pKsIMap->eotfParam).bdp = (unsigned short)hMdsExt->signal_bit_depth;
        eotf_or_sample = hMdsExt->signal_eotf;
    }
    if (eotf_or_sample == 0xffff) {
        (pKsIMap->eotfParam).eotf = '\x01';
    } else {
        (pKsIMap->eotfParam).eotf = '\0';

        if ((unsigned short)(hMdsExt->source_max_PQ - 0xe6f) < 3) {
            if ((unsigned short)(hMdsExt->source_min_PQ - 0x3c) < 3) {
                range_or_lut_offset = 0;
                do {
                    next_lut_byte_offset = range_or_lut_offset + 4;
                    *(unsigned int *)((long long)pKsIMap->g2L + range_or_lut_offset) =
                        *(unsigned int *)((long long)g2lPulsar256Lut + range_or_lut_offset);
                    range_or_lut_offset = next_lut_byte_offset;
                } while (next_lut_byte_offset != 0x400);
                goto commit_ipt_normalization;
            }
        } else if (((unsigned short)(hMdsExt->source_max_PQ - 0x820) < 3) &&
                   ((unsigned short)(hMdsExt->source_min_PQ - 0x3c) < 3)) {
            range_or_lut_offset = 0;
            do {
                next_lut_byte_offset = range_or_lut_offset + 4;
                *(unsigned int *)((long long)pKsIMap->g2L + range_or_lut_offset) =
                    *(unsigned int *)((long long)g2lSdr256Lut + range_or_lut_offset);
                range_or_lut_offset = next_lut_byte_offset;
            } while (next_lut_byte_offset != 0x400);
            goto commit_ipt_normalization;
        }
        gamma_lut_cursor = pKsIMap->g2L;
        eotf_or_sample = 0;
        do {

            range_or_gamma_value = dovi_dm_DeGamma(hMdsExt->signal_eotf_param0, hMdsExt->signal_eotf_param1,
                            hMdsExt->signal_eotf, hMdsExt->signal_eotf_param2, eotf_or_sample);
            next_gamma_lut_word = gamma_lut_cursor + 1;
            *gamma_lut_cursor = range_or_gamma_value;
            gamma_lut_cursor = next_gamma_lut_word;
            eotf_or_sample = eotf_or_sample + 0x100;
        } while (next_gamma_lut_word != (unsigned int *)&pKsIMap->m33Rgb2LmsScale2P);
    }
commit_ipt_normalization:
    dovi_AssignM33I2I(m33Lms2Ipt, pKsIMap->m33Lms2Ipt);
    pKsIMap->m33Lms2IptScale2P = (short)lms_to_ipt_scale;
    dovi_GetCvmBypassNormParam((unsigned int)hMdsExt->signal_bit_depth,
                    (unsigned int)(hMdsExt->v3Yuv2Rgb[0] == 0), pKsIMap->v3IptOff,
                    &pKsIMap->iptScale);
    pKsIMap->v3IptOff[0] = (unsigned int)*(unsigned short *)((long long)hMdsExt->v3Yuv2Rgb + 2);
    pKsIMap->v3IptOff[1] = (unsigned int)*(unsigned short *)((long long)hMdsExt->v3Yuv2Rgb + 6);
    pKsIMap->v3IptOff[2] = (unsigned int)*(unsigned short *)((long long)hMdsExt->v3Yuv2Rgb + 10);
    return;
}

void commit_graphics_input(DmCtxtFxp_t *context, DmCfgFxp_t *config, DmKsFxp_t *kernel)
{
    short (*working_matrix)[3];
    long long next_lut_byte_offset;
    unsigned int graphics_bit_depth;
    unsigned int graphics_color_space;
    unsigned char alpha_bit_depth;
    unsigned short sA;
    unsigned short depth_or_pq_sample;
    unsigned short minimum_pq_or_offset;
    unsigned int range_or_gamma_scale;
    unsigned int gamma_value;
    int depth_or_matrix_scale;
    unsigned char kernel_color;
    short derived_matrix_scale;
    unsigned int range_mode;
    int external_matrix_scale;
    long long range_or_lut_offset;
    DmKsFxp_t *active_kernel;
    unsigned int *gamma_lut_cursor;
    unsigned int *next_gamma_lut_word;
    short (*lms_to_ipt)[3];
    int gamma_a;
    int gamma_b;
    int offset[3];

    lms_to_ipt = (context->cscCtrl).m33Lms2Ipt;
    dovi_dm_CommitCdm2KsFrmFmt((SigEnvCom_t *)&config->grcSigEnv, &kernel->ksFrmFmtG);
    graphics_bit_depth = (config->grcSigEnv).Bdp;
    active_kernel = context->hDmKs;
    (context->cscCtrl).gV3Wp[2] = 0x7fff;
    (context->cscCtrl).gV3Wp[1] = 0x7fff;
    depth_or_pq_sample = (unsigned short)graphics_bit_depth;
    depth_or_matrix_scale = (int)(short)depth_or_pq_sample;
    (context->cscCtrl).gV3Wp[0] = 0x7fff;
    (context->cscCtrl).gV3WpScale2P = 0xf;
    if ((config->grcSigEnv).WpExt != 0) {
        dovi_MemCpyByte((context->cscCtrl).gV3Wp, (config->grcSigEnv).V3Wp, 0xc);
        (context->cscCtrl).gV3WpScale2P = (int)(config->grcSigEnv).WpScale;
    }
    graphics_color_space = (config->grcSigEnv).Clr;
    kernel_color = '\0';
    if (graphics_color_space != CClrYuv) {
        if (graphics_color_space == CClrRgb) {
            kernel_color = '\x01';
        } else {
            kernel_color = '\x02';
            if (graphics_color_space != CClrRgba) {
                kernel_color = '\0';
            }
        }
    }
    (active_kernel->ksIMapG).clr = kernel_color;
    if ((config->grcSigEnv).Clr == CClrIpt) {
        dovi_dm_CommitIpt2KsIMap((config->grcSigEnv).Bdp, (config->grcSigEnv).Rng, &active_kernel->ksIMapG);
        if ((config->grcSigEnv).Yuv2RgbExt != 0) {
            dovi_AssignM33I2I((config->grcSigEnv).M33Yuv2Rgb, (active_kernel->ksIMapG).m33Yuv2Rgb);
            (active_kernel->ksIMapG).m33Yuv2RgbScale2P =
                (short)(config->grcSigEnv).M33Yuv2RgbScale2P;
        }
        if ((config->grcSigEnv).Yuv2RgbOffExt != 0) {
            dovi_AssignV3I2I((config->grcSigEnv).V3Yuv2RgbOff, offset);
            offset[0] = fixed_signed32((unsigned int)offset[0] << 16);
            offset[1] = fixed_signed32((unsigned int)offset[1] << 16);
            dovi_Yuv2RgbOffsetRgbFxp((active_kernel->ksIMapG).m33Yuv2Rgb, offset[0], offset[1],
                                (active_kernel->ksIMapG).v3Yuv2RgbOffInRgb);
        }
        if ((config->grcSigEnv).Rgb2LmsM33Ext != 0) {
            working_matrix = (context->cscCtrl).gM33Rgb2Lms;
            dovi_MemCpyByte(working_matrix, (config->grcSigEnv).M33Rgb2Lms, 0x12);
            external_matrix_scale = (config->grcSigEnv).M33Rgb2LmsScale2P;
            (context->cscCtrl).gM33Rgb2LmsScale2P = external_matrix_scale;
            derived_matrix_scale = dovi_GetRgb2LmsCtWpM33Fxp(
                working_matrix, (short)external_matrix_scale, (config->grcSigEnv).CrossTalk, (context->cscCtrl).gV3Wp,
                (short)(context->cscCtrl).gV3WpScale2P, (active_kernel->ksIMapG).m33Rgb2Lms);
            (active_kernel->ksIMapG).m33Rgb2LmsScale2P = derived_matrix_scale;
        }
    } else {
        if ((config->grcSigEnv).Yuv2RgbExt == 0) {
            working_matrix = (active_kernel->ksIMapG).m33Yuv2Rgb;
            if ((config->grcSigEnv).Rng == CRngNarrow) {
                derived_matrix_scale = dovi_GetYuv2RgbM33NarrowFxp((config->grcSigEnv).YuvXferSpec,
                                                (config->grcSigEnv).Bdp, working_matrix);
                (active_kernel->ksIMapG).m33Yuv2RgbScale2P = derived_matrix_scale;
                goto check_graphics_yuv_offset;
            }
            derived_matrix_scale = dovi_GetYuv2RgbM33Fxp((config->grcSigEnv).YuvXferSpec, working_matrix);
            (active_kernel->ksIMapG).m33Yuv2RgbScale2P = derived_matrix_scale;
            if ((config->grcSigEnv).Yuv2RgbOffExt == 0)
                goto derive_graphics_yuv_offset;
        copy_graphics_yuv_offset:
            dovi_AssignV3I2I((config->grcSigEnv).V3Yuv2RgbOff, offset);
        } else {
            dovi_AssignM33I2I((config->grcSigEnv).M33Yuv2Rgb, (active_kernel->ksIMapG).m33Yuv2Rgb);
            (active_kernel->ksIMapG).m33Yuv2RgbScale2P =
                (short)(config->grcSigEnv).M33Yuv2RgbScale2P;
        check_graphics_yuv_offset:
            if ((config->grcSigEnv).Yuv2RgbOffExt != 0)
                goto copy_graphics_yuv_offset;
        derive_graphics_yuv_offset:
            dovi_GetYuvRgbOffFxp((config->grcSigEnv).Rng, depth_or_matrix_scale, offset);
        }
        offset[1] = fixed_signed32((unsigned int)offset[1] << 16);
        offset[0] = fixed_signed32((unsigned int)offset[0] << 16);
        dovi_Yuv2RgbOffsetRgbFxp((active_kernel->ksIMapG).m33Yuv2Rgb, offset[0], offset[1],
                            (active_kernel->ksIMapG).v3Yuv2RgbOffInRgb);
        range_mode = CRngFull;
        if ((config->grcSigEnv).Clr - CClrRgb < 2) {
            range_mode = (config->grcSigEnv).Rng;
        }
        dovi_GetRange(depth_or_matrix_scale, range_mode, &(active_kernel->ksIMapG).eotfParam.rangeMin,
                 &(active_kernel->ksIMapG).eotfParam.range);
        range_or_gamma_scale = 0xffffffff;
        if (depth_or_matrix_scale != 0) {

            range_or_lut_offset = (long long)(int)((unsigned int)(active_kernel->ksIMapG).eotfParam.range
                                      << (unsigned long long)((0x10U - depth_or_matrix_scale) & 0x1f));
            range_or_gamma_scale = 0;
            if (range_or_lut_offset != 0) {
                range_or_gamma_scale = (unsigned int)(0xffffffff / range_or_lut_offset);
            }
        }
        (active_kernel->ksIMapG).eotfParam.rangeInv = range_or_gamma_scale;
        (active_kernel->ksIMapG).eotfParam.bdp = depth_or_pq_sample;
        (active_kernel->ksIMapG).eotfParam.eotf = (config->grcSigEnv).Eotf == CEotfPq;
        if ((config->grcSigEnv).Eotf == CEotfBt1886) {

            calc_ta_tb((unsigned int)(config->grcSigEnv).Gamma * 2, (config->grcSigEnv).Max,
                     (config->grcSigEnv).Min, &gamma_a, &gamma_b);
            gamma_a = fixed_signed32((unsigned int)gamma_a + 0x4000u) >> 15;
            gamma_b = fixed_signed32((unsigned int)gamma_b + 0x100u) >> 9;
            range_or_gamma_scale = dovi_fxPOW2Taylor((unsigned int)(config->grcSigEnv).Gamma, 0xe, 0x12);

            depth_or_matrix_scale = dm_pair((config->grcSigEnv).MinPq, (config->grcSigEnv).MaxPq);
            if (depth_or_matrix_scale == 0) {
                depth_or_pq_sample = dovi_LToPQ12((config->grcSigEnv).Max);
                minimum_pq_or_offset = dovi_LToPQ12((config->grcSigEnv).Min);
            } else {
                depth_or_pq_sample = (config->grcSigEnv).MaxPq;
                minimum_pq_or_offset = (config->grcSigEnv).MinPq;
            }
            if ((unsigned short)(depth_or_pq_sample - 0xe6f) < 3) {
                if ((unsigned short)(minimum_pq_or_offset - 0x3c) < 3) {
                    range_or_lut_offset = 0;
                    do {
                        next_lut_byte_offset = range_or_lut_offset + 4;
                        *(unsigned int *)((long long)(active_kernel->ksIMapG).g2L + range_or_lut_offset) =
                            *(unsigned int *)((long long)g2lPulsar256Lut + range_or_lut_offset);
                        range_or_lut_offset = next_lut_byte_offset;
                    } while (next_lut_byte_offset != 0x400);
                } else {
                build_graphics_degamma:
                    sA = (unsigned short)gamma_a;
                    depth_or_pq_sample = 0;
                    minimum_pq_or_offset = (unsigned short)gamma_b;
                    gamma_lut_cursor = (active_kernel->ksIMapG).g2L;
                    do {

                        gamma_value = dovi_dm_DeGamma(sA, minimum_pq_or_offset, (config->grcSigEnv).Gamma, range_or_gamma_scale, depth_or_pq_sample);
                        next_gamma_lut_word = gamma_lut_cursor + 1;
                        *gamma_lut_cursor = gamma_value;
                        depth_or_pq_sample = depth_or_pq_sample + 0x100;
                        gamma_lut_cursor = next_gamma_lut_word;
                    } while (next_gamma_lut_word !=
                             (unsigned int *)&(active_kernel->ksIMapG).m33Rgb2LmsScale2P);
                }
            } else {
                if ((2 < (unsigned short)(depth_or_pq_sample - 0x820)) || (2 < (unsigned short)(minimum_pq_or_offset - 0x3c)))
                    goto build_graphics_degamma;
                range_or_lut_offset = 0;
                do {
                    next_lut_byte_offset = range_or_lut_offset + 4;
                    *(unsigned int *)((long long)(active_kernel->ksIMapG).g2L + range_or_lut_offset) =
                        *(unsigned int *)((long long)g2lSdr256Lut + range_or_lut_offset);
                    range_or_lut_offset = next_lut_byte_offset;
                } while (next_lut_byte_offset != 0x400);
            }
        }
        if ((config->grcSigEnv).Rgb2LmsM33Ext == 0) {
            if ((config->grcSigEnv).Rgb2LmsRgbwExt == 0) {
                derived_matrix_scale =
                    dovi_GetRgb2LmsByDefM33((config->grcSigEnv).RgbDef, (context->cscCtrl).gM33Rgb2Lms);
                depth_or_matrix_scale = (int)derived_matrix_scale;
                (context->cscCtrl).gM33Rgb2LmsScale2P = depth_or_matrix_scale;
            } else {
                derived_matrix_scale = dovi_GetRgb2LmsByPrimsM33Fxp(
                    (config->grcSigEnv).V8Rgbw[0], (config->grcSigEnv).V8Rgbw[1],
                    (config->grcSigEnv).V8Rgbw[2], (config->grcSigEnv).V8Rgbw[3],
                    (config->grcSigEnv).V8Rgbw[4], (config->grcSigEnv).V8Rgbw[5],
                    (config->grcSigEnv).V8Rgbw[6], (config->grcSigEnv).V8Rgbw[7],
                    (context->cscCtrl).gM33Rgb2Lms);
                depth_or_matrix_scale = (int)derived_matrix_scale;
                (context->cscCtrl).gM33Rgb2LmsScale2P = depth_or_matrix_scale;
            }
        } else {
            dovi_MemCpyByte((context->cscCtrl).gM33Rgb2Lms, (config->grcSigEnv).M33Rgb2Lms, 0x12);
            depth_or_matrix_scale = (config->grcSigEnv).M33Rgb2LmsScale2P;
            (context->cscCtrl).gM33Rgb2LmsScale2P = depth_or_matrix_scale;
        }
        derived_matrix_scale = dovi_GetRgb2LmsCtWpM33Fxp((context->cscCtrl).gM33Rgb2Lms, (short)depth_or_matrix_scale,
                                      (config->grcSigEnv).CrossTalk, (context->cscCtrl).gV3Wp,
                                      (short)(context->cscCtrl).gV3WpScale2P,
                                      (active_kernel->ksIMapG).m33Rgb2Lms);
        (active_kernel->ksIMapG).m33Rgb2LmsScale2P = derived_matrix_scale;
    }
    (active_kernel->ksIMapG).m33Lms2IptScale2P = (short)(context->cscCtrl).m33Lms2IptScale2P;
    dovi_AssignM33I2I(lms_to_ipt, (active_kernel->ksIMapG).m33Lms2Ipt);
    dovi_GetCvmBypassNormParam((config->grcSigEnv).Bdp, (config->grcSigEnv).Rng,
                    (active_kernel->ksIMapG).v3IptOff, &(active_kernel->ksIMapG).iptScale);
    if (((config->dmCtrl).SeparatedAlpha != 0) && ((config->grcSigEnv).Clr == CClrRgba)) {
        (context->hDmKs->ksIMapG).clr = '\x01';
    }
    dovi_dm_CommitCdm2KsFrmFmt(&config->alpSigEnv, &kernel->ksFrmFmtA);
    alpha_bit_depth = (kernel->ksFrmFmtA).bdp;
    if (alpha_bit_depth == 0) {
        (kernel->ksIMapG).eotfParam.bdp = 0;
        range_or_gamma_scale = 0x10000;
        kernel->rangeInvA = 0x10000;
        (kernel->ksIMapG).eotfParam.rangeMin = 0;
    } else {
        (kernel->ksIMapG).eotfParam.bdp = (unsigned short)alpha_bit_depth;
        (kernel->ksIMapG).eotfParam.rangeMin = 0;

        range_or_lut_offset = fixed_signed32(((1u << (alpha_bit_depth & 31u)) - 1u) << ((16u - alpha_bit_depth) & 31u));
        range_or_gamma_scale = 0;
        if (range_or_lut_offset != 0) {
            range_or_gamma_scale = (unsigned int)(0xffffffff / range_or_lut_offset);
        }
        kernel->rangeInvA = range_or_gamma_scale;
    }
    (kernel->ksIMapG).eotfParam.rangeInv = range_or_gamma_scale;
}

void commit_video_input(DmCtxtFxp_t *context, DmCfgFxp_t *config, DmKsFxp_t *kernel)
{
    CscCtrlFxp_t *color;
    unsigned int video_color_space;
    unsigned short gamma_sample;
    unsigned int range_or_gamma_value;
    int depth_or_matrix_scale;
    unsigned char kernel_color;
    short derived_matrix_scale;
    unsigned int range_mode;
    int external_matrix_scale;
    long long range_denominator;
    DmKsFxp_t *active_kernel;
    unsigned int *gamma_lut_cursor;
    unsigned int *next_gamma_lut_word;
    short (*lms_to_ipt)[3];
    int offset[3];

    lms_to_ipt = (context->cscCtrl).m33Lms2Ipt;
    color = &context->cscCtrl;
    dovi_dm_CommitCdm2KsFrmFmt((SigEnvCom_t *)&config->srcSigEnv, &kernel->ksFrmFmtI);

    active_kernel = context->hDmKs;
    (context->cscCtrl).sV3Wp[2] = 0x7fff;
    (context->cscCtrl).sV3Wp[1] = 0x7fff;
    (context->cscCtrl).sV3Wp[0] = 0x7fff;
    (context->cscCtrl).sV3WpScale2P = 0xf;
    if ((config->dmCtrl).MdsFollowed == 0) {
        depth_or_matrix_scale = (config->srcSigEnv).Bdp;
        if ((config->srcSigEnv).WpExt != 0) {
            dovi_MemCpyByte((context->cscCtrl).sV3Wp, (config->srcSigEnv).V3Wp, 0xc);
            (context->cscCtrl).sV3WpScale2P = (int)(config->srcSigEnv).WpScale;
        }
        video_color_space = (config->srcSigEnv).Clr;
        kernel_color = '\0';
        if (video_color_space != CClrYuv) {
            if (video_color_space == CClrRgb) {
                kernel_color = '\x01';
            } else {
                kernel_color = '\x02';
                if (video_color_space != CClrRgba) {
                    kernel_color = '\0';
                }
            }
        }
        (active_kernel->ksIMap).clr = kernel_color;
        if ((config->srcSigEnv).Clr == CClrIpt) {
            dovi_dm_CommitIpt2KsIMap((config->srcSigEnv).Bdp, (config->srcSigEnv).Rng,
                            &active_kernel->ksIMap);
            if ((config->srcSigEnv).Yuv2RgbExt != 0) {
                dovi_AssignM33I2I((config->srcSigEnv).M33Yuv2Rgb, (active_kernel->ksIMap).m33Yuv2Rgb);
                (active_kernel->ksIMap).m33Yuv2RgbScale2P =
                    (short)(config->srcSigEnv).M33Yuv2RgbScale2P;
            }
            if ((config->srcSigEnv).Yuv2RgbOffExt != 0) {
                dovi_AssignV3I2I((config->srcSigEnv).V3Yuv2RgbOff, offset);
                offset[0] = fixed_signed32((unsigned int)offset[0] << 16);
                offset[1] = fixed_signed32((unsigned int)offset[1] << 16);
                dovi_Yuv2RgbOffsetRgbFxp((active_kernel->ksIMap).m33Yuv2Rgb, offset[0], offset[1],
                                    (active_kernel->ksIMap).v3Yuv2RgbOffInRgb);
            }
            if ((config->srcSigEnv).Rgb2LmsM33Ext != 0) {
                dovi_MemCpyByte(color, (config->srcSigEnv).M33Rgb2Lms, 0x12);
                external_matrix_scale = (config->srcSigEnv).M33Rgb2LmsScale2P;
                (context->cscCtrl).sM33Rgb2LmsScale2P = external_matrix_scale;
                derived_matrix_scale = dovi_GetRgb2LmsCtWpM33Fxp(
                    color, (short)external_matrix_scale, (config->srcSigEnv).CrossTalk, (context->cscCtrl).sV3Wp,
                    (short)(context->cscCtrl).sV3WpScale2P, (active_kernel->ksIMap).m33Rgb2Lms);
                (active_kernel->ksIMap).m33Rgb2LmsScale2P = derived_matrix_scale;
            }
        } else {
            if ((config->srcSigEnv).Yuv2RgbExt == 0) {
                if ((config->srcSigEnv).Rng == CRngNarrow) {
                    derived_matrix_scale = dovi_GetYuv2RgbM33NarrowFxp((config->srcSigEnv).YuvXferSpec,
                                                    (config->srcSigEnv).Bdp,
                                                    (active_kernel->ksIMap).m33Yuv2Rgb);
                    (active_kernel->ksIMap).m33Yuv2RgbScale2P = derived_matrix_scale;
                } else {
                    derived_matrix_scale = dovi_GetYuv2RgbM33Fxp((config->srcSigEnv).YuvXferSpec,
                                              (active_kernel->ksIMap).m33Yuv2Rgb);
                    (active_kernel->ksIMap).m33Yuv2RgbScale2P = derived_matrix_scale;
                }
            } else {
                dovi_AssignM33I2I((config->srcSigEnv).M33Yuv2Rgb, (active_kernel->ksIMap).m33Yuv2Rgb);
                (active_kernel->ksIMap).m33Yuv2RgbScale2P =
                    (short)(config->srcSigEnv).M33Yuv2RgbScale2P;
            }
            if ((config->srcSigEnv).Yuv2RgbOffExt == 0) {
                dovi_GetYuvRgbOffFxp((config->srcSigEnv).Rng, depth_or_matrix_scale, offset);
            } else {
                dovi_AssignV3I2I((config->srcSigEnv).V3Yuv2RgbOff, offset);
            }
            offset[1] = fixed_signed32((unsigned int)offset[1] << 16);
            offset[0] = fixed_signed32((unsigned int)offset[0] << 16);
            dovi_Yuv2RgbOffsetRgbFxp((active_kernel->ksIMap).m33Yuv2Rgb, offset[0], offset[1],
                                (active_kernel->ksIMap).v3Yuv2RgbOffInRgb);
            range_mode = CRngFull;
            if ((config->srcSigEnv).Clr - CClrRgb < 2) {
                range_mode = (config->srcSigEnv).Rng;
            }
            dovi_GetRange(depth_or_matrix_scale, range_mode, &(active_kernel->ksIMap).eotfParam.rangeMin,
                     &(active_kernel->ksIMap).eotfParam.range);
            range_or_gamma_value = 0xffffffff;
            if (depth_or_matrix_scale != 0) {

                range_denominator = (long long)(int)((unsigned int)(active_kernel->ksIMap).eotfParam.range
                                          << (unsigned long long)((0x10U - depth_or_matrix_scale) & 0x1f));
                range_or_gamma_value = 0;
                if (range_denominator != 0) {
                    range_or_gamma_value = (unsigned int)(0xffffffff / range_denominator);
                }
            }
            (active_kernel->ksIMap).eotfParam.rangeInv = range_or_gamma_value;
            (active_kernel->ksIMap).eotfParam.bdp = (unsigned short)depth_or_matrix_scale;
            (active_kernel->ksIMap).eotfParam.eotf = (config->srcSigEnv).Eotf == CEotfPq;
            if ((config->srcSigEnv).Eotf == CEotfBt1886) {
                gamma_sample = 0;
                gamma_lut_cursor = (active_kernel->ksIMap).g2L;
                do {

                    range_or_gamma_value = dovi_dm_DeGamma((config->srcSigEnv).A, (config->srcSigEnv).B,
                                     (config->srcSigEnv).Gamma, (config->srcSigEnv).G, gamma_sample);
                    next_gamma_lut_word = gamma_lut_cursor + 1;
                    *gamma_lut_cursor = range_or_gamma_value;
                    gamma_sample = gamma_sample + 0x100;
                    gamma_lut_cursor = next_gamma_lut_word;
                } while (next_gamma_lut_word != (unsigned int *)&(active_kernel->ksIMap).m33Rgb2LmsScale2P);
            }
            if ((config->srcSigEnv).Rgb2LmsM33Ext == 0) {
                if ((config->srcSigEnv).Rgb2LmsRgbwExt == 0) {
                    derived_matrix_scale = dovi_GetRgb2LmsByDefM33((config->srcSigEnv).RgbDef, color);
                    depth_or_matrix_scale = (int)derived_matrix_scale;
                    (context->cscCtrl).sM33Rgb2LmsScale2P = depth_or_matrix_scale;
                } else {
                    derived_matrix_scale = dovi_GetRgb2LmsByPrimsM33Fxp(
                        (config->srcSigEnv).V8Rgbw[0], (config->srcSigEnv).V8Rgbw[1],
                        (config->srcSigEnv).V8Rgbw[2], (config->srcSigEnv).V8Rgbw[3],
                        (config->srcSigEnv).V8Rgbw[4], (config->srcSigEnv).V8Rgbw[5],
                        (config->srcSigEnv).V8Rgbw[6], (config->srcSigEnv).V8Rgbw[7], color);
                    depth_or_matrix_scale = (int)derived_matrix_scale;
                    (context->cscCtrl).sM33Rgb2LmsScale2P = depth_or_matrix_scale;
                }
            } else {
                dovi_MemCpyByte(color, (config->srcSigEnv).M33Rgb2Lms, 0x12);
                depth_or_matrix_scale = (config->srcSigEnv).M33Rgb2LmsScale2P;
                (context->cscCtrl).sM33Rgb2LmsScale2P = depth_or_matrix_scale;
            }
            derived_matrix_scale = dovi_GetRgb2LmsCtWpM33Fxp(
                color, (short)depth_or_matrix_scale, (config->srcSigEnv).CrossTalk, (context->cscCtrl).sV3Wp,
                (short)(context->cscCtrl).sV3WpScale2P, (active_kernel->ksIMap).m33Rgb2Lms);
            (active_kernel->ksIMap).m33Rgb2LmsScale2P = derived_matrix_scale;
        }
    }
    (active_kernel->ksIMap).m33Lms2IptScale2P = (short)(context->cscCtrl).m33Lms2IptScale2P;
    dovi_AssignM33I2I(lms_to_ipt, (active_kernel->ksIMap).m33Lms2Ipt);
    dovi_GetCvmBypassNormParam((config->srcSigEnv).Bdp, (config->srcSigEnv).Rng,
                    (active_kernel->ksIMap).v3IptOff, &(active_kernel->ksIMap).iptScale);
}

void dovi_dm_CommitIpt2KsIMap(unsigned int bits, unsigned int range, void *map)
{
    unsigned char *out = (unsigned char *)map;
    void *matrix = out + 4u;
    int offsets[3], scale;
    short matrix_scale;
    out[0] = 0u;
    matrix_scale = range == 0u ? dovi_GetIpt2LmsM33NarrowFxp((short)bits, matrix)
                               : dovi_GetIpt2LmsM33Fxp(matrix);
    fixed_store_le16((unsigned char *)map + (2u), (unsigned short)matrix_scale);
    dovi_GetCvmBypassNormParam((int)bits, range, offsets, &scale);
    dovi_Yuv2RgbOffsetRgbFxp(matrix, (unsigned int)offsets[0], (unsigned int)offsets[1], out + 24u);
    fixed_store_le16((unsigned char *)map + (36u), 0u);
    fixed_store_le16((unsigned char *)map + (1074u + 16u), 1u);
    out[46] = 1u;
    fixed_store_le16((unsigned char *)map + (1074u + 8u), 1u);
    fixed_store_le16((unsigned char *)map + (1074u), 1u);
    fixed_store_le32(
        (unsigned char *)map + (40u),
        (unsigned int)kernel_divide_max((int)kernel_shift_left((unsigned int)scale, 16u - bits)));
    fixed_store_le16((unsigned char *)map + (1074u + 14u), 0u);
    fixed_store_le16((unsigned char *)map + (1074u + 12u), 0u);
    fixed_store_le16((unsigned char *)map + (1074u + 10u), 0u);
    fixed_store_le16((unsigned char *)map + (1074u + 6u), 0u);
    fixed_store_le16((unsigned char *)map + (1074u + 4u), 0u);
    fixed_store_le16((unsigned char *)map + (1074u + 2u), 0u);
    fixed_store_le16((unsigned char *)map + (1072u), 0u);
}
