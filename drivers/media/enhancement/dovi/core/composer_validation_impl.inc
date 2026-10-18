/* Validate composer and display metadata fields and report diagnostics. */

typedef unsigned char byte;
typedef unsigned int u32;
typedef char require_uint32[(sizeof(u32) == 4 && sizeof(int) == 4) ? 1 : -1];

static u32 read16(const byte *p)
{
    return (u32)p[0] | ((u32)p[1] << 8);
}

static u32 read32(const byte *p)
{
    return read16(p) | (read16(p + 2) << 16);
}

static int signed32(u32 value)
{
    return (value & 0x80000000u) ? -1 - (int)~value : (int)value;
}

static u32 sign_extend16(u32 value)
{
    return value | (0u - (value & 0x8000u));
}

struct validation {
    validation_log_fn log;
    u32 frame;
    u32 level;
    int result;
};

enum diagnostic { RANGE, MAXIMUM, CHOICES, ONLY };

static void invalid(struct validation *v, enum diagnostic kind, const char *field, u32 value,
                    u32 low, u32 high)
{
    v->result = -2;
    if (v->log == (validation_log_fn)0 || v->level < 3u)
        return;
    if (kind == RANGE)
        v->log("Frame #%d  %s is %d, allowed values are %d..%d\n", signed32(v->frame), field,
               signed32(value), signed32(low), signed32(high));
    else if (kind == MAXIMUM)
        v->log("Frame #%d  %s is %d, allowed values are 0..%d\n", signed32(v->frame), field,
               signed32(value), signed32(high));
    else if (kind == CHOICES)
        v->log("Frame #%d  %s is %d, allowed values are %d, %d\n", signed32(v->frame), field,
               signed32(value), signed32(low), signed32(high));
    else
        v->log("Frame #%d  %s is %d the only allowed value is %d\n", signed32(v->frame), field,
               signed32(value), signed32(high));
}

int dovi_validate_dm_payload(const void *dm, u32 frame, validation_log_fn log,
                             validation_log_level log_level)
{
    const byte *p = (const byte *)dm;
    struct validation v = {log, frame, (u32)log_level, 0};
    u32 value, eotf, count, i;
    if (p[0] != 0)
        invalid(&v, MAXIMUM, "dmp->affected_dm_metadata_id", p[0], 0, 0);
    if (p[1] != 0)
        invalid(&v, MAXIMUM, "dmp->current_dm_metadata_id", p[1], 0, 0);
    if (p[2] > 1u)
        invalid(&v, MAXIMUM, "dmp->scene_refresh_flag", p[2], 0, 1);
    eotf = read16(p + 0x36);
    if (eotf < 0xfffeu && ((eotf + 0x7333u) & 0xffffu) > 0x2666u) {
        v.result = -2;
        if (log != (validation_log_fn)0 && (u32)log_level >= 3u)
            log("Frame #%d  %s is %d, allowed values are %d .. %d, or %d, %d\n", signed32(frame),
                "signal_eotf", eotf, 0x8ccd, 0xb333, 0xfffe, 0xffff);
        eotf = read16(p + 0x36);
    }
    if (eotf < 0xfffeu) {
        value = read16(p + 0x38);
        if (((value - 0x55u) & 0xffffu) > 0x9bd7u)
            invalid(&v, RANGE, "dmp->signal_eotf_param0", value, 0x55, 0x9c2c);
        value = read16(p + 0x3a);
        if (value - 0x55c3u < 0xffffaa4cu)
            invalid(&v, RANGE, "dmp->signal_eotf_param1", value, 0xf, 0x55c2);
        value = read32(p + 0x3c);
        if (value - 0x1bdb8eu < 0xfff68584u)
            invalid(&v, RANGE, "dmp->signal_eotf_param2", value, 0x126112, 0x1bdb8d);
    } else {
        value = read16(p + 0x38);
        if (value != 0)
            invalid(&v, MAXIMUM, "dmp->signal_eotf_param0", value, 0, 0);
        value = read16(p + 0x3a);
        if (value != 0)
            invalid(&v, MAXIMUM, "dmp->signal_eotf_param1", value, 0, 0);
        value = read32(p + 0x3c);
        if (value != 0)
            invalid(&v, MAXIMUM, "dmp->signal_eotf_param2", value, 0, 0);
    }
    value = p[0x40];
    if (value - 17u < 0xfffffff7u)
        invalid(&v, RANGE, "dmp->signal_bit_depth", value, 8, 16);
    if (p[0x41] != 1 && p[0x43] != 1) {
        v.result = -2;
        if (log != (validation_log_fn)0 && (u32)log_level >= 3u)
            log("signal_full_range_flag is 0, allowed value is 1 if signal_color_space != 1\n");
    }
    if (p[0x42] > 2u)
        invalid(&v, MAXIMUM, "dmp->signal_chroma_format", p[0x42], 0, 2);
    if (read16(p + 0x36) != 0xffffu) {
        value = read16(p + 0x44);
        if (value - 0x267u < 0xfffffda0u)
            invalid(&v, RANGE, "dmp->source_min_PQ", value, 7, 0x266);
        value = read16(p + 0x46);
        if (value - 0x1000u < 0xfffff6fbu)
            invalid(&v, RANGE, "dmp->source_max_PQ", value, 0x6fb, 0xfff);
    }
    count = p[0x4a];
    if (count == 255u) {
        invalid(&v, MAXIMUM, "dmp->num_ext_blocks", count, 0, 254);
        count = p[0x4a];
    }
    for (i = 0; i < count; ++i) {
        const byte *record = p + 0x4c + i * 0x108u;
        if (record[4] == 2u) {
            value = read16(record + 0x12);
            if (((value + 1u) & 0xffffu) > 0x1000u) {
                invalid(&v, RANGE, "dmp->ext_block[i].l.level_2.ms_weight", sign_extend16(value),
                        0xffffffffu, 0xfff);
                if (log != (validation_log_fn)0 && (u32)log_level >= 3u)
                    count = p[0x4a];
            }
        }
    }
    return v.result;
}

static void maximum(struct validation *v, const char *field, u32 value, u32 limit)
{
    if (value > limit)
        invalid(v, MAXIMUM, field, value, 0, limit);
}

static void fraction(struct validation *v, const char *field, u32 value, u32 width)
{
    u32 shift = width & 31u;
    if ((value >> shift) != 0)
        invalid(v, MAXIMUM, field, value, 0, ~(0xffffffffu << shift));
}

static void coefficient(struct validation *v, const char *field, u32 value, u32 bias,
                        u32 maximum_value)
{
    if (value + bias > maximum_value)
        invalid(v, RANGE, field, value, 0u - bias, maximum_value - bias);
}

int dovi_validate_composer_config(const void *composer, u32 frame, validation_log_fn log,
                                  validation_log_level log_level)
{
    const byte *p = (const byte *)composer;
    struct validation v = {log, frame, (u32)log_level, 0};
    u32 value, i, j, mode, expected;
    static const char *const poly_int[3] = {
        "efrc->poly_coef_int[cmp][i][0]", "efrc->poly_coef_int[cmp][i][1]",
        "efrc->poly_coef_int[cmp][i][2]"};
    static const char *const poly_frac[3] = {
        "efrc->poly_coef [cmp][i][0]", "efrc->poly_coef [cmp][i][1]",
        "efrc->poly_coef [cmp][i][2]"};
    static const char *const nlq_int[3] = {
        "efrc->NLQ_coeff_int[cmp][0]", "efrc->NLQ_coeff_int[cmp][1]",
        "efrc->NLQ_coeff_int[cmp][2]"};
    static const char *const nlq_frac[3] = {
        "efrc->NLQ_coeff [cmp][0]", "efrc->NLQ_coeff [cmp][1]",
        "efrc->NLQ_coeff [cmp][2]"};
    static const char *const pivots[3] = {
        "efrc->num_pivots[0]", "efrc->num_pivots[1]", "efrc->num_pivots[2]"};
    static const char *const hor_filter[3] = {
        "efrc->spatial_resampling_filter_hor_idc[0]",
        "efrc->spatial_resampling_filter_hor_idc[1]",
        "efrc->spatial_resampling_filter_hor_idc[2]"};
    static const char *const ver_filter[3] = {
        "efrc->spatial_resampling_filter_ver_idc[0]",
        "efrc->spatial_resampling_filter_ver_idc[1]",
        "efrc->spatial_resampling_filter_ver_idc[2]"};

    value = read32(p);
    if (value - 15u < 0xfffffff9u)
        invalid(&v, RANGE, "efrc->rpu_VDR_bit_depth", value, 8, 14);
    value = read32(p + 4);
    if ((value | 2u) != 10u)
        invalid(&v, CHOICES, "efrc->rpu_BL_bit_depth", value, 8, 10);
    value = read32(p + 8);
    if (p[0x49d] == 0 && (value | 2u) != 10u)
        invalid(&v, CHOICES, "efrc->rpu_EL_bit_depth", value, 8, 10);
    value = read32(p + 0xc);
    if (value > 23u || value < read32(p + 8) + 5u)
        invalid(&v, RANGE, "efrc->coefficient_log2_denom", value, read32(p + 8) + 5u, 23);
    value = read32(p + 0x10);
    if (value - 10u < 0xfffffff8u)
        invalid(&v, RANGE, pivots[0], value, 2, 9);
    for (i = 1; i < 3u; ++i) {
        value = read32(p + 0x10 + i * 4u);
        if (read32(p + 0x88 + i * 4u) == 1u) {
            if (value != 2u)
                invalid(&v, ONLY, pivots[i], value, 0, 2);
        } else if (value - 6u < 0xfffffffcu) {
            invalid(&v, RANGE, pivots[i], value, 2, 5);
        }
    }
    for (i = 0x1c; i < 0x88u; i += 4u)
        maximum(&v, "efrc->pivot_value[cmp][i]", read32(p + i), 1u << (read32(p + 4) & 31u));
    for (i = 0x88; i < 0x94u; i += 4u)
        maximum(&v, "efrc->mapping_idc[cmp]", read32(p + i), 1);
    for (i = 0x94; i < 0xf4u; i += 4u)
        maximum(&v, "efrc->poly_order[cmp][i]", read32(p + i), 2);
    for (i = 0; i < 72u; ++i) {
        j = i % 3u;
        coefficient(&v, poly_int[j], read32(p + 0xf4 + i * 4u), 0x40, 0x7f);
        fraction(&v, poly_frac[j], read32(p + 0x214 + i * 4u), read32(p + 0xc));
    }
    for (i = 0x334; i < 0x33cu; i += 4u)
        maximum(&v, "efrc->MMR_order[cmp]", read32(p + i), 3);
    for (i = 0; i < 44u; ++i) {
        coefficient(&v, "efrc->MMR_coef_int[cmp][i]", read32(p + 0x33c + i * 4u), 0x10000, 0x1ffff);
        fraction(&v, "efrc->MMR_coef [cmp][i]", read32(p + 0x3ec + i * 4u), read32(p + 0xc));
    }
    maximum(&v, "efrc->NLQ_method_idc", p[0x49c], 0);
    maximum(&v, "efrc->disable_residual_flag", p[0x49d], 1);
    maximum(&v, "efrc->el_spatial_resampling_filter_flag", p[0x49e], 1);
    if (p[0x49f] > 1u)
        invalid(&v, CHOICES, "efrc->BL_video_full_range_flag", p[0x49f], 0, 1);
    for (i = 0; i < 3u; ++i)
        fraction(&v, "efrc->NLQ_offset[cmp]", read32(p + 0x4a0 + i * 4u), read32(p + 8));
    for (i = 0; i < 9u; ++i) {
        value = read32(p + 0x4ac + i * 4u);
        if (signed32(value) > 1)
            invalid(&v, MAXIMUM, nlq_int[i % 3u], value, 0, 1);
        fraction(&v, nlq_frac[i % 3u], read32(p + 0x4d0 + i * 4u), read32(p + 0xc));
    }
    maximum(&v, "efrc->spatial_resampling_filter_flag", read32(p + 0x4f4), 1);
    maximum(&v, "efrc->spatial_resampling_explicit_filter_flag", read32(p + 0x4f8), 1);
    value = read32(p + 0x4fc);
    if (value != 0 && value - 21u < 0xfffffff1u)
        invalid(&v, RANGE, "efrc->spatial_filter_exp_coef_log2_denom", value, 6, 20);
    maximum(&v, "efrc->spatial_resampling_mode_hor_idc", read32(p + 0x500), 1);
    maximum(&v, "efrc->spatial_resampling_mode_ver_idc", read32(p + 0x504), 3);
    value = read32(p + 0x508);
    if (value != 0 && value != 7u)
        invalid(&v, CHOICES, hor_filter[0], value, 0, 7);
    for (i = 1; i < 3u; ++i) {
        value = read32(p + 0x508 + i * 4u);
        if (value != 0)
            invalid(&v, ONLY, hor_filter[i], value, 0, 0);
    }
    mode = read32(p + 0x504);
    expected = mode - 2u < 2u ? 1u : 0u;
    value = read32(p + 0x514);
    if (mode - 1u < 3u) {
        if (value != expected && value != 7u)
            invalid(&v, CHOICES, ver_filter[0], value, expected, 7);
    } else if (value != 0) {
        invalid(&v, ONLY, ver_filter[0], value, 0, 0);
    }
    for (i = 1; i < 3u; ++i) {
        value = read32(p + 0x514 + i * 4u);
        if (value != expected)
            invalid(&v, ONLY, ver_filter[i], value, 0, expected);
    }
    fraction(&v, "efrc->spatial_resampling_luma_pivot[0]", read32(p + 0x520), read32(p));
    fraction(&v, "efrc->spatial_resampling_luma_pivot[1]", read32(p + 0x524), read32(p));
    for (i = 0; i < 24u; ++i) {
        coefficient(&v, "efrc->spatial_filter_coeff_hor_int[i][j]", read32(p + 0x528 + i * 4u),
                    0x40, 0x7f);
        fraction(&v, "efrc->spatial_filter_coeff_hor [i][j]", read32(p + 0x588 + i * 4u),
                 read32(p + 0x4fc));
    }
    for (i = 0; i < 36u; ++i) {
        const char *field_int = i < 18u ? "efrc->spatial_filter_coeff_ver_int[0][i][j]"
                                        : "efrc->spatial_filter_coeff_ver_int[1][i][j]";
        const char *field_frac = i < 18u ? "efrc->spatial_filter_coeff_ver [0][i][j]"
                                         : "efrc->spatial_filter_coeff_ver [1][i][j]";
        coefficient(&v, field_int, read32(p + 0x5e8 + i * 4u), 0x40, 0x7f);
        fraction(&v, field_frac, read32(p + 0x678 + i * 4u), read32(p + 0x4fc));
    }
    return v.result;
}
