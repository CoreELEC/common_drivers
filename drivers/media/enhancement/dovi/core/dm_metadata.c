/* Input display metadata decoding and configuration. */
#include "dm_pipeline.h"
#include "dm_bindings.h"
#include "byte_utils.h"
#include "pq_conversion.h"
#include "pq_conversion_tables.h"
#include "fixed_access.h"

/* These readers are the unchecked bodies used after caller length checks.
 * Keep pointer/output/count write order: their storage can alias. */
static int read_metadata_u8(const unsigned char **cursor, int *remaining, unsigned char *output)
{
    const unsigned char *source = *cursor;
    *cursor = source + 1;
    *output = *source;
    *remaining = fixed_signed32((unsigned int)*remaining - 1u);
    return 1;
}

static int read_metadata_u32(const unsigned char **cursor, int *remaining, unsigned int *output)
{
    const unsigned char *source = *cursor;
    fixed_store_le32(output, fixed_load_be32(source));
    *cursor = *cursor + 4;
    *remaining = fixed_signed32((unsigned int)*remaining - 4u);
    return 1;
}

static int read_metadata_u16(const unsigned char **cursor, int *remaining, unsigned short *output)
{
    const unsigned char *source;
    if (*remaining < 2)
        return 0;
    source = *cursor;
    *output = (unsigned short)fixed_load_be16(source);
    *cursor += 2;
    *remaining -= 2;
    return 1;
}

int dovi_dm_ParseMds(const unsigned char *input, int size, MdsExt_t *metadata,
                     int output_window_mode, const DmCfgFxp_t *configuration)
{
    const unsigned char *cursor = input;
    unsigned char level, block_index;
    int remaining = size, i;
    unsigned int length;
    unsigned short source_min, source_max;
    (void)output_window_mode;
    if (remaining == 0)
        return -1;
    read_metadata_u8(&cursor, &remaining, &metadata->affected_dm_metadata_id);
    if (remaining == 0)
        return -1;
    read_metadata_u8(&cursor, &remaining, &metadata->scene_refresh_flag);
    metadata->m33Yuv2RgbScale2P = 13;
    if (remaining < 18)
        return -1;
    for (i = 0; i < 9; ++i)
        read_metadata_u16(&cursor, &remaining, (unsigned short *)metadata->m33Yuv2Rgb + i);
    if (remaining < 12)
        return -1;
    for (i = 0; i < 3; ++i)
        read_metadata_u32(&cursor, &remaining, metadata->v3Yuv2Rgb + i);
    metadata->m33Rgb2WpLmsScale2P = 14;
    if (remaining < 18)
        return -1;
    for (i = 0; i < 9; ++i)
        read_metadata_u16(&cursor, &remaining, (unsigned short *)metadata->m33Rgb2WpLms + i);
    if (!read_metadata_u16(&cursor, &remaining, &metadata->signal_eotf) ||
        !read_metadata_u16(&cursor, &remaining, &metadata->signal_eotf_param0) ||
        !read_metadata_u16(&cursor, &remaining, &metadata->signal_eotf_param1))
        return -1;
    if (remaining < 4)
        return -1;
    read_metadata_u32(&cursor, &remaining, &metadata->signal_eotf_param2);
    if (remaining == 0)
        return -1;
    read_metadata_u8(&cursor, &remaining, &metadata->signal_bit_depth);
    if (remaining == 0)
        return -1;
    read_metadata_u8(&cursor, &remaining, &metadata->signal_color_space);
    if (remaining == 0)
        return -1;
    read_metadata_u8(&cursor, &remaining, &metadata->signal_chroma_format);
    if (remaining == 0)
        return -1;
    read_metadata_u8(&cursor, &remaining, &metadata->signal_full_range_flag);
    if (!read_metadata_u16(&cursor, &remaining, &metadata->source_min_PQ))
        return -1;
    source_min = metadata->source_min_PQ;
    if (!read_metadata_u16(&cursor, &remaining, &metadata->source_max_PQ))
        return -1;
    source_max = metadata->source_max_PQ;
    if (remaining < 2)
        return -1;
    metadata->min_PQ = source_min;
    metadata->max_PQ = source_max;
    metadata->mid_PQ = (unsigned short)(((unsigned int)source_min + source_max) >> 1);
    read_metadata_u16(&cursor, &remaining, &metadata->source_diagonal);
    metadata->trimSets.TrimSetNum = 0;
    metadata->trimSets.TrimSets[0].TrimNum = 0;
    *(unsigned short *)metadata->trimSets.TrimSets[0].Trima = source_max;
    metadata->trimSets.TrimSets[1].TrimNum = 0;
    *(unsigned short *)metadata->trimSets.TrimSets[1].Trima = metadata->source_max_PQ;
    metadata->dm_debug1 = 1;
    metadata->dm_debug2 = 2;
    metadata->dm_debug3 = 3;
    metadata->lvl5AoiAvail = 0;
    metadata->active_area_left_offset = 0;
    metadata->active_area_right_offset = 0;
    metadata->active_area_top_offset = 0;
    metadata->active_area_bottom_offset = 0;
    metadata->lvl255RunModeAvail = 0;
    metadata->dm_run_mode = 0;
    metadata->dm_run_version = 0;
    metadata->dm_debug0 = 0;
    if (remaining == 0)
        return -1;
    read_metadata_u8(&cursor, &remaining, &metadata->num_ext_blocks);
    for (block_index = 0; block_index < metadata->num_ext_blocks; ++block_index) {
        if (remaining < 4)
            return -1;
        read_metadata_u32(&cursor, &remaining, &length);
        if (remaining == 0)
            return -1;
        read_metadata_u8(&cursor, &remaining, &level);
        if (level == 1) {
            if (!read_metadata_u16(&cursor, &remaining, &metadata->min_PQ) ||
                !read_metadata_u16(&cursor, &remaining, &metadata->max_PQ) ||
                !read_metadata_u16(&cursor, &remaining, &metadata->mid_PQ))
                return -1;
            length -= 6;
        } else if (level == 3 || (level == 2 && configuration->tmCtrl.l2off == 0)) {
            Trim_t *trim = metadata->trimSets.TrimSets + level - 2;
            unsigned short *words;
            int count = trim->TrimNum, dimension;
            if (count < trim->TrimNumMax) {
                dimension = trim->TrimTypeDim;
                count = fixed_signed16((unsigned int)count + 1u);
                trim->TrimNum = (short)count;
                if (remaining < dimension * 2)
                    return -1;
                words = trim->Trima;
                for (i = 0; i < dimension; ++i) {
                    if (remaining > 1) {
                        unsigned short value =
                            (unsigned short)fixed_load_be16(cursor);
                        words[(long long)i + (long long)count * dimension] = value;
                        words = trim->Trima;
                        count = trim->TrimNum;
                        cursor += 2;
                        remaining -= 2;
                    }
                    dimension = trim->TrimTypeDim;
                }
                if ((words[(long long)count * dimension + 6] & 0x1fff) == 0x1fff)
                    words[(long long)count * dimension + 6] = words[6];
                length -= (unsigned int)trim->TrimTypeDim * 2u;
            }
            metadata->trimSets.TrimSetNum = (short)((metadata->trimSets.TrimSets[0].TrimNum != 0) +
                                                    (metadata->trimSets.TrimSets[1].TrimNum != 0));
        } else if (level == 5) {
            metadata->lvl5AoiAvail = 1;
            if (!read_metadata_u16(&cursor, &remaining, &metadata->active_area_left_offset) ||
                !read_metadata_u16(&cursor, &remaining, &metadata->active_area_right_offset) ||
                !read_metadata_u16(&cursor, &remaining, &metadata->active_area_top_offset) ||
                !read_metadata_u16(&cursor, &remaining, &metadata->active_area_bottom_offset))
                return -1;
            length -= 8;
        } else if (level == 255) {
            unsigned char *run = &metadata->dm_run_mode;
            metadata->lvl255RunModeAvail = 1;
            for (i = 0; i < 6; ++i) {
                if (remaining == 0)
                    return -1;
                read_metadata_u8(&cursor, &remaining, run + i);
            }
            length -= 6;
        }
        if (fixed_signed32(length) < 0 || remaining < fixed_signed32(length))
            return -1;
        remaining -= (int)length;
        cursor += length;
    }
    return fixed_signed32((unsigned int)size - (unsigned int)remaining);
}

int commit_dynamic_metadata(MdsExt_t *metadata, DmCtxtFxp_t *context, unsigned int commit_word)
{
    TcCtrlFxp_t *video_curve;
    TcCtrlFxp_t *graphics_curve;
    unsigned int *pDmCtrl;
    TmCtrlFxp_t *pTmCtrl;
    int video_active_area_pair;
    int graphics_active_area_pair;
    int active_source_pq_bounds;
    int incoming_source_pq_bounds;
    unsigned short pq_endpoint_a;
    unsigned short pq_endpoint_b;
    unsigned short graphics_or_video_bound;
    unsigned short graphics_source_min_pq;
    unsigned char kernel_chroma;
    unsigned short clip_or_endpoint_pq;
    unsigned short crush_or_endpoint_pq;
    unsigned short metadata_mid_pq;
    unsigned short diagonal_or_trim_pq;
    unsigned int output_minimum_linear;
    unsigned int tone_result_or_pq_index;
    unsigned int metadata_change;
    unsigned int output_maximum_linear;
    unsigned int tone_direction;
    int pitch_or_trim_limit;
    DmKsFxp_t *kernel;
    int update_output_transfer;
    unsigned int target_maximum_pq;
    DmCfgFxp_t *config;
    unsigned int commit_flags;

    video_curve = &context->tcCtrl;
    graphics_curve = &context->tc2Ctrl;
    metadata_mid_pq = metadata->mid_PQ;
    clip_or_endpoint_pq = metadata->max_PQ;
    crush_or_endpoint_pq = metadata->min_PQ;
    config = context->pDmCfgAct;
    kernel = context->hDmKs;
    if ((context->mdsExtFrcCmmt == 0) && (context->mdsExtActValid != 0)) {
        metadata_change = dovi_dm_CheckMds(metadata, &context->mdsExtAct);
        if (metadata_change == MDS_CHG_NONE) {
            return commit_word;
        }
        commit_flags = commit_word | 2;
        if (metadata_change == MDS_CHG_CFG)
            goto update_metadata_format;
    } else {
    update_metadata_format:
        (kernel->ksFrmFmtI).dtp = '\0';
        if (metadata->signal_chroma_format == '\x01') {
            (kernel->ksFrmFmtI).weav = '\x02';
            if (metadata->signal_chroma_format == '\0') {
                pitch_or_trim_limit = 6;
            } else {
                pitch_or_trim_limit = 4;
                if (metadata->signal_chroma_format != '\x01') {
                    pitch_or_trim_limit = 6;
                }
            }
        } else {
            (kernel->ksFrmFmtI).weav = '\0';

            pitch_or_trim_limit = 2;
        }
        (kernel->ksFrmFmtI).colPitch = pitch_or_trim_limit;
        (kernel->ksUs).minUs = 0;
        (kernel->ksUs).maxUs =
            (short)(1u << (unsigned long long)(metadata->signal_bit_depth & 0x1f)) + -1;
        kernel_chroma = metadata->signal_chroma_format;
        if ((kernel_chroma != '\0') && (kernel_chroma != '\x01')) {
            kernel_chroma = '\x02';
        }
        (kernel->ksUs).chrmIn = kernel_chroma;
        (kernel->ksFrmFmtI).chrm = kernel_chroma;
        commit_flags = commit_word | 6;
        dovi_dm_CommitMdsCsc(metadata, config, &context->cscCtrl, &kernel->ksIMap);
        (kernel->ksFrmFmtI).bdp = metadata->signal_bit_depth;
    }
    pq_endpoint_a = metadata->source_min_PQ;
    (context->tcCtrl).tcSrcSig.minPq = pq_endpoint_a;
    pq_endpoint_b = metadata->source_max_PQ;
    (context->tcCtrl).tcSrcSig.maxPq = pq_endpoint_b;
    diagonal_or_trim_pq = metadata->source_diagonal;
    (context->tcCtrl).tcSrcSig.diagSize = diagonal_or_trim_pq;
    commit_word = (unsigned int)pq_endpoint_a;
    if ((config->dmCtrl).StbMode == CStbModeVpri) {
        (context->tcCtrl).tcTgtSig.minPq = pq_endpoint_a;
        (context->tcCtrl).tcTgtSig.maxPq = pq_endpoint_b;
        (context->tcCtrl).tcTgtSig.diagSize = diagonal_or_trim_pq;
        tone_result_or_pq_index = commit_word;
        pq_endpoint_a = pq_endpoint_b;
    } else {
        tone_result_or_pq_index = (unsigned int)(context->tcCtrl).tcTgtSig.minPq;
        pq_endpoint_a = (context->tcCtrl).tcTgtSig.maxPq;
    }
    target_maximum_pq = (unsigned int)pq_endpoint_a;
    tone_direction = CTcLutDirBwd;
    if (target_maximum_pq <= pq_endpoint_b + 1u) {
        tone_direction = CTcLutDirFwd;
        if (((unsigned int)pq_endpoint_b <= target_maximum_pq + 1) &&
            ((commit_word + 1 < tone_result_or_pq_index || (tone_direction = CTcLutDirPass, tone_result_or_pq_index + 1 < commit_word)))) {
            tone_direction = (unsigned int)(pq_endpoint_b < target_maximum_pq);
        }
    }
    pDmCtrl = &(config->dmCtrl).StbMode;
    pTmCtrl = &config->tmCtrl;
    (context->tcCtrl).tcLutDir = tone_direction;
    tm_ctrl_to_tc_ctrl(pTmCtrl, (DmCtrlFxp_t *)pDmCtrl, video_curve, 0);
    if ((context->tcCtrl).tcLutDir == CTcLutDirBwd) {
        diagonal_or_trim_pq = metadata->source_max_PQ;
    } else {
        diagonal_or_trim_pq = (context->tcCtrl).tcTgtSig.maxPq;
    }
    dovi_dm_CommitTrim(diagonal_or_trim_pq, metadata, (DmCtxtFxp_t *)context->pDmCfgAct, video_curve);
    if (((config->dmCtrl).SecIn != 0) && ((config->dmCtrl).StbMode == CStbModeVpri)) {
        diagonal_or_trim_pq = dovi_LToPQ12((config->grcSigEnv).Max);
        pq_endpoint_a = clip_or_endpoint_pq;
        if (clip_or_endpoint_pq <= diagonal_or_trim_pq) {
            pq_endpoint_a = diagonal_or_trim_pq;
        }
        diagonal_or_trim_pq = dovi_LToPQ12((config->grcSigEnv).Min);
        graphics_or_video_bound = (context->tcCtrl).tcSrcSig.minPq;
        pq_endpoint_b = crush_or_endpoint_pq;
        if (diagonal_or_trim_pq <= crush_or_endpoint_pq) {
            pq_endpoint_b = diagonal_or_trim_pq;
        }
        clip_or_endpoint_pq = (context->tcCtrl).tcSrcSig.maxPq;
        if (pq_endpoint_a <= clip_or_endpoint_pq) {
            clip_or_endpoint_pq = pq_endpoint_a;
        }
        crush_or_endpoint_pq = graphics_or_video_bound;
        if (graphics_or_video_bound <= pq_endpoint_b) {
            crush_or_endpoint_pq = pq_endpoint_b;
        }
    }
    (context->tcCtrl).tcSrcSig.crush = crush_or_endpoint_pq;
    (context->tcCtrl).tcSrcSig.clip = clip_or_endpoint_pq;
    (context->tcCtrl).tcSrcSig.mid = metadata_mid_pq;
    pq_endpoint_a = (context->tcCtrl).tcTgtSig.minPq;
    pq_endpoint_b = (context->tcCtrl).tcTgtSig.maxPq;
    (context->tcCtrl).tcTgtSig.crush = pq_endpoint_a;
    (context->tcCtrl).tcTgtSig.clip = pq_endpoint_b;
    commit_word = (int)((unsigned int)pq_endpoint_a + (unsigned int)pq_endpoint_b) >> 1;
    (context->tcCtrl).tcTgtSig.mid = (unsigned short)commit_word;
    if ((metadata->lvl5AoiAvail != 0) &&
        ((video_active_area_pair = dm_pair(metadata->active_area_right_offset, metadata->active_area_top_offset),
          video_active_area_pair != 0 || (metadata->active_area_bottom_offset != 0)))) {
        pitch_or_trim_limit = (context->tcCtrl).level2[2];
        if (0 < pitch_or_trim_limit) {
            pitch_or_trim_limit = 0;
        }
        (context->tcCtrl).level2[2] = pitch_or_trim_limit;
    }
    tone_result_or_pq_index = dovi_CreateToneCurve(video_curve, &kernel->ksTMap, &context->tcCtrlUsed);
    commit_word = commit_flags;
    if (tone_result_or_pq_index != 0) {
        commit_word = commit_flags | 0x100010;
        if ((tone_result_or_pq_index & 0x100000) == 0) {
            commit_word = commit_flags | 0x10;
        }
    }
    commit_flags = commit_word;
    if ((config->dmCtrl).SecIn != 0) {
        if ((config->dmCtrl).StbMode == CStbModeVpri) {
            pq_endpoint_a = (context->tcCtrl).tcTgtSig.minPq;
            (context->tc2Ctrl).tcTgtSig.minPq = pq_endpoint_a;
            pq_endpoint_b = (context->tcCtrl).tcTgtSig.maxPq;
            (context->tc2Ctrl).tcTgtSig.maxPq = pq_endpoint_b;
            (context->tc2Ctrl).tcTgtSig.diagSize = (context->tcCtrl).tcTgtSig.diagSize;
            graphics_or_video_bound = (context->tc2Ctrl).tcSrcSig.maxPq;
            graphics_source_min_pq = (context->tc2Ctrl).tcSrcSig.minPq;
            tone_direction = CTcLutDirBwd;
            if ((unsigned int)pq_endpoint_b <= graphics_or_video_bound + 1u) {
                tone_result_or_pq_index = pq_endpoint_b + 1;
                tone_direction = CTcLutDirFwd;
                if ((unsigned int)graphics_or_video_bound == tone_result_or_pq_index || (unsigned int)graphics_or_video_bound < tone_result_or_pq_index) {
                    if ((graphics_source_min_pq + 1u < (unsigned int)pq_endpoint_a) ||
                        (tone_direction = CTcLutDirPass, (unsigned int)graphics_source_min_pq != pq_endpoint_a + 1u &&
                                                     pq_endpoint_a + 1u <= (unsigned int)graphics_source_min_pq)) {
                        tone_direction = (unsigned int)((unsigned int)graphics_or_video_bound < (unsigned int)pq_endpoint_b);
                    }
                }
            }
            (context->tc2Ctrl).tcLutDir = tone_direction;
            tm_ctrl_to_tc_ctrl(pTmCtrl, (DmCtrlFxp_t *)pDmCtrl, graphics_curve, 1);
            if (((config->dmCtrl).StbMode != CStbModeVpri) ||
                ((context->tc2Ctrl).tcLutDir != CTcLutDirBwd))
                goto update_graphics_tone_level;
            dovi_dm_CommitTrim((context->tc2Ctrl).tcSrcSig.maxPq, metadata,
                       (DmCtxtFxp_t *)context->pDmCfgAct, graphics_curve);
            if ((metadata->lvl5AoiAvail != 0) &&
                ((graphics_active_area_pair =
                      dm_pair(metadata->active_area_right_offset, metadata->active_area_top_offset),
                  graphics_active_area_pair != 0 || (metadata->active_area_bottom_offset != 0)))) {
                pitch_or_trim_limit = (context->tc2Ctrl).level2[2];
                if (0 < pitch_or_trim_limit) {
                    pitch_or_trim_limit = 0;
                }
                (context->tc2Ctrl).level2[2] = pitch_or_trim_limit;
            }
        } else {
        update_graphics_tone_level:
            update_tc_ctrl_level(pTmCtrl, graphics_curve);
        }
        if ((config->dmCtrl).StbMode == CStbModeVpri) {
            (context->tc2Ctrl).tcTgtSig.crush = crush_or_endpoint_pq;
            (context->tc2Ctrl).tcTgtSig.clip = clip_or_endpoint_pq;
            (context->tc2Ctrl).tcTgtSig.mid = metadata_mid_pq;
        } else {
            clip_or_endpoint_pq = (context->tc2Ctrl).tcTgtSig.minPq;
            crush_or_endpoint_pq = (context->tc2Ctrl).tcTgtSig.maxPq;
            (context->tc2Ctrl).tcTgtSig.crush = clip_or_endpoint_pq;
            (context->tc2Ctrl).tcTgtSig.clip = crush_or_endpoint_pq;
            (context->tc2Ctrl).tcTgtSig.mid =
                (unsigned short)((int)((unsigned int)clip_or_endpoint_pq + (unsigned int)crush_or_endpoint_pq) >> 1);
        }
        tone_result_or_pq_index = dovi_CreateToneCurve(graphics_curve, &kernel->ksTMapG, &context->tc2CtrlUsed);
        if (tone_result_or_pq_index != 0) {
            commit_flags = commit_word | 0x200020;
            if ((tone_result_or_pq_index & 0x100000) == 0) {
                commit_flags = commit_word | 0x20;
            }
        }
    }
    update_output_transfer = 1;
    if ((config->dmCtrl).StbMode == CStbModeVpri) {
        if (((context->mdsExtActValid != 0) && (context->mdsExtFrcCmmt == 0)) &&
            (active_source_pq_bounds =
                 dm_pair((context->mdsExtAct).source_min_PQ, (context->mdsExtAct).source_max_PQ),
             incoming_source_pq_bounds = dm_pair(metadata->source_min_PQ, metadata->source_max_PQ), active_source_pq_bounds == incoming_source_pq_bounds))
            goto publish_metadata;

        clip_or_endpoint_pq = (context->tcCtrl).tcSrcSig.minPq;
        commit_word = (unsigned int)clip_or_endpoint_pq;
        if (0xfff < clip_or_endpoint_pq) {
            commit_word = 0xfff;
        }
        tone_result_or_pq_index = commit_word >> 2;
        output_minimum_linear = pq_depq1024Lut[(int)tone_result_or_pq_index];
        if (tone_result_or_pq_index != 0x3ff) {
            output_minimum_linear =
                output_minimum_linear +
                (int)((long long)((unsigned long long)(pq_depq1024Lut[(int)(tone_result_or_pq_index + 1)] - output_minimum_linear) *
                                  ((unsigned long long)commit_word & 3)) >>
                      2);
        }

        clip_or_endpoint_pq = (context->tcCtrl).tcSrcSig.maxPq;
        commit_word = (unsigned int)clip_or_endpoint_pq;
        if (0xfff < clip_or_endpoint_pq) {
            commit_word = 0xfff;
        }
        tone_result_or_pq_index = commit_word >> 2;
        output_maximum_linear = pq_depq1024Lut[(int)tone_result_or_pq_index];
        if (tone_result_or_pq_index != 0x3ff) {
            output_maximum_linear =
                output_maximum_linear +
                (int)((long long)((unsigned long long)(pq_depq1024Lut[(int)(tone_result_or_pq_index + 1)] - output_maximum_linear) *
                                  ((unsigned long long)commit_word & 3)) >>
                      2);
        }
        (kernel->ksOMap).oetfParam.min = output_minimum_linear;
        (kernel->ksOMap).oetfParam.max = output_maximum_linear;
        if (((config->tgtSigEnv).Eotf == CEotfPq) || ((config->tgtSigEnv).Clr == CClrIpt)) {
            (kernel->ksOMap).oetfParam.min = 0;
            (kernel->ksOMap).oetfParam.max = 0x9c400000;
            update_output_transfer = 0;
        } else {

            clip_or_endpoint_pq = (context->tcCtrl).tcSrcSig.minPq;
            commit_word = (unsigned int)clip_or_endpoint_pq;
            if (0xfff < clip_or_endpoint_pq) {
                commit_word = 0xfff;
            }
            tone_result_or_pq_index = commit_word >> 2;
            output_minimum_linear = pq_depq1024Lut[(int)tone_result_or_pq_index];
            if (tone_result_or_pq_index != 0x3ff) {
                output_minimum_linear = output_minimum_linear +
                         (int)((long long)((unsigned long long)(pq_depq1024Lut[(int)(tone_result_or_pq_index + 1)] -
                                                                output_minimum_linear) *
                                           ((unsigned long long)commit_word & 3)) >>
                               2);
            }

            clip_or_endpoint_pq = (context->tcCtrl).tcSrcSig.maxPq;
            commit_word = (unsigned int)clip_or_endpoint_pq;
            if (0xfff < clip_or_endpoint_pq) {
                commit_word = 0xfff;
            }
            tone_result_or_pq_index = commit_word >> 2;
            output_maximum_linear = pq_depq1024Lut[(int)tone_result_or_pq_index];
            if (tone_result_or_pq_index != 0x3ff) {
                output_maximum_linear = output_maximum_linear +
                         (int)((long long)((unsigned long long)(pq_depq1024Lut[(int)(tone_result_or_pq_index + 1)] -
                                                                output_maximum_linear) *
                                           ((unsigned long long)commit_word & 3)) >>
                               2);
            }
            (kernel->ksOMap).oetfParam.min = output_minimum_linear;
            update_output_transfer = 0;
            (kernel->ksOMap).oetfParam.max = output_maximum_linear;
        }
    }
    if (context->mdsExtFrcCmmt != 0) {
        dovi_dm_CommitSigEnv2KsOMap(config, update_output_transfer, context);
    }
publish_metadata:
    dovi_MemCpyByte(&context->mdsExtAct, metadata, 0x208);
    context->mdsExtFrcCmmt = 0;
    context->mdsExtActValid = 1;
    return commit_flags;
}
