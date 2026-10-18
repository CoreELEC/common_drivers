#include "tone_parameters.h"
#include "fixed_arithmetic.h"
#include "fixed_transcendentals.h"
#include "pq_conversion.h"
#include "fixed_access.h"

/* Use explicit 64-bit intermediates, wrapping and masked AArch64 shifts. */
static int add32(int a, int b)
{
    return (int)((unsigned int)a + (unsigned int)b);
}

static int sub32(int a, int b)
{
    return (int)((unsigned int)a - (unsigned int)b);
}

static int neg32(int a)
{
    return (int)(0U - (unsigned int)a);
}

static int shl32(int a, unsigned int n)
{
    return (int)((unsigned int)a << (n & 31U));
}

static long long shl64(long long a, unsigned int n)
{
    return (long long)((unsigned long long)a << (n & 63U));
}

static long long add64(long long a, long long b)
{
    return (long long)((unsigned long long)a + (unsigned long long)b);
}

static long long sub64(long long a, long long b)
{
    return (long long)((unsigned long long)a - (unsigned long long)b);
}

static long long mul64(long long a, long long b)
{
    return (long long)((unsigned long long)a * (unsigned long long)b);
}

static int sat32(long long value)
{
    if (value > 2147483647LL)
        return 2147483647;
    if (value < (-2147483647LL - 1))
        return (-2147483647 - 1);
    return (int)value;
}

static int high32(long long value, unsigned int shift)
{
    return (int)((unsigned long long)shl64(value, shift) >> 32);
}

static long long product_shift(int a, int b, unsigned int shift)
{
    return ((long long)a * b) >> (shift & 63U);
}

void dovi_CalcMappedParams2pxx(int crush, int mid, int clip, struct tone_curve_display *display,
                               int source_min_pq, int source_max_pq, int source_diagonal,
                               int *target_min, int *target_mid, int *target_max, int *source_mid,
                               int *slope)
{
    int input_min, input_max, input_diagonal, output_min, output_max;
    int value, inverse, contrast, offset, upper, lower;
    int screen_ratio;
    unsigned int ratio, norm, shift, root;
    long long wide, screen;
    unsigned long long product;

    if (display->mode == 1) {
        input_min = display->min_pq;
        input_diagonal = display->diagonal_inches;
        input_max = display->max_pq;
        display->min_pq = source_min_pq;
        display->diagonal_inches = source_diagonal;
        display->max_pq = source_max_pq;
        output_max = source_max_pq;
        output_min = source_min_pq;
    } else {
        output_max = display->max_pq;
        output_min = display->min_pq;
        input_min = source_min_pq;
        input_diagonal = source_diagonal;
        input_max = source_max_pq;
    }
    value = (int)(((long long)input_min + input_max + 1) >> 1);
    norm = dovi_unorm_s(value);
    inverse = dovi_sdiv_sls(0x2000000000000000ll, shl32(value, norm));
    shift = 30u - norm;
    if (fixed_signed32(shift) > 63)
        shift = 63;
    wide = product_shift((int)(((long long)output_min + output_max + 1) >> 1), inverse, shift);
    ratio = dovi_ssqrt_s((unsigned int)sat32(wide));
    if (display->diagonal_inches == input_diagonal) {
        screen = 0x8000000;
        screen_ratio = 0x8000000;
    } else {
        value = dovi_fxLOG2Taylor(display->diagonal_inches, 26);
        inverse = dovi_fxLOG2Taylor(input_diagonal, 26);
        norm = dovi_unorm_s(inverse);
        inverse = dovi_sdiv_sls(0x2000000000000000ll, shl32(inverse, norm));
        shift = 34u - norm;
        if (fixed_signed32(shift) > 63)
            shift = 63;
        wide = product_shift(inverse, value, shift);
        root = dovi_ssqrt_s((unsigned int)sat32(wide));
        screen_ratio = dovi_sshr_sss(fixed_signed32(root), 2);
        screen = screen_ratio;
    }
    norm = dovi_unorm_s(fixed_signed32(ratio));
    inverse = dovi_sdiv_sls(0x2000000000000000ll, shl32(fixed_signed32(ratio), norm));
    shift = 7u - norm;
    if (fixed_signed32(shift) > 31)
        shift = 31;
    root = dovi_ssqrt_s((unsigned int)(inverse >> (shift & 31u)));
    wide = mul64(fixed_signed32(root) >> 4, screen) >> 20;
    value = sat32(wide);
    contrast = dovi_smpy_ss(value, display->contrast);
    *slope = dovi_sadd_sss(value, contrast);
    value = dovi_smpy_ss(sub32(0x7fffffff, fixed_signed32(ratio)), screen_ratio);
    norm = dovi_unorm_s(sub32(input_max, input_min));
    inverse = dovi_sdiv_sls(0x2000000000000000ll, shl32(sub32(input_max, input_min), norm));
    product = (unsigned long long)((long long)inverse * sub32(mid, input_min));
    root = dovi_unorm_ul(product);
    shift = root - norm + 24u;
    if (fixed_signed32(shift) > 63)
        shift = 63;
    wide = mul64(high32(fixed_signed64(product), root), value) >> (shift & 63u);
    offset = dovi_smpy_ss(sat32(wide), mid);
    offset = dovi_sadd_sss(offset, display->brightness);

    value = dovi_ssub_sss(crush, offset);
    lower = display->min_pq;
    if (lower < value)
        lower = dovi_ssub_sss(crush, offset);
    *target_min = dovi_sadd_sss(lower, display->min_bias);
    value = dovi_ssub_sss(clip, offset);
    upper = display->max_pq;
    if (value < upper)
        upper = dovi_ssub_sss(clip, offset);
    value = dovi_sadd_sss(upper, display->max_bias);
    *target_max = value;
    upper = dovi_sadd_sss(sub32(mid, offset), display->mid_bias);
    if (value < upper)
        value = *target_max;
    else
        value = dovi_sadd_sss(sub32(mid, offset), display->mid_bias);
    if (value < *target_min)
        value = *target_min;
    *source_mid = dovi_sadd_sss(value, sub32(offset, display->mid_bias));
    value = dovi_smpy_ss(sub32(clip, crush), 0x6ccccccc);
    upper = dovi_sadd_sss(value, crush);
    value = dovi_smpy_ss(sub32(clip, crush), 0x13333333);
    lower = dovi_sadd_sss(value, crush);
    if (*source_mid < upper)
        upper = *source_mid;
    if (upper < lower)
        upper = lower;
    *source_mid = upper;
    upper = dovi_sadd_sss(dovi_smpy_ss(sub32(*target_max, *target_min), 0x6ccccccc), *target_min);
    lower = dovi_sadd_sss(dovi_smpy_ss(sub32(*target_max, *target_min), 0x13333333), *target_min);
    value = dovi_sadd_sss(sub32(*source_mid, offset), display->mid_bias);
    if (upper < value)
        value = upper;
    if (value < lower)
        value = lower;
    *target_mid = value;
}

void calc_mapped_params(int source_max, struct tone_curve_source *source,
                        struct tone_curve_display *display, int *target_min, int *target_mid,
                        int *target_max, int *source_mid, int *slope, int *source_target_ratio,
                        int *screen_ratio)
{
    int key_weight = source->key_weight, crush = source->crush, mid = source->mid;
    int input_min, input_max, output_min, output_max, input_diagonal, output_diagonal;
    int preserve = display->bright_preserve;
    int input_middle, output_middle, difference, value, other, offset, upper, lower;
    unsigned int norm, shift;
    long long wide, ease, square;
    if (display->mode == 1) {
        input_min = source->min_pq;
        input_max = source->max_pq;
        output_min = display->min_pq;
        output_max = display->max_pq;
        input_diagonal = source->diagonal_inches;
        output_diagonal = display->diagonal_inches;
    } else {
        output_min = source->min_pq;
        output_max = source->max_pq;
        output_diagonal = source->diagonal_inches;
        input_min = display->min_pq;
        input_max = display->max_pq;
        input_diagonal = display->diagonal_inches;
    }
    output_middle = add32(output_min >> 1, output_max >> 1);
    input_middle = add32(input_min >> 1, input_max >> 1);
    difference = sub32(output_middle, input_middle);
    if (preserve != 0) {
        value = add32(crush >> 1, source_max >> 1);
        if (value < input_middle)
            wide = 0;
        else if (difference < 1 || output_middle < value)
            wide = (long long)difference * preserve;
        else {
            norm = dovi_unorm_s(difference);
            other = dovi_sshl_ssu(difference, norm);
            value = other ? (int)(shl64((long long)sub32(value, input_middle), 30) / other) : 0;
            ease = 0;
            if (shl32(1, 29U - norm) <= value) {
                ease = 0x100000;
                value = sub32(value, shl32(1, 30U - norm));
            }
            square = product_shift(value, value, 31);
            preserve = display->bright_preserve;
            square = mul64(square, square) >> 31;
            square = mul64(value, square) >> (((30U - norm) * 5U - 117U) & 63U);
            wide = mul64(difference, add64(square, ease)) >> 20;
            wide = (long long)sat32(wide) * preserve;
        }
        wide = add64(wide, (long long)sub32(0x1000, preserve) * difference);
        difference = (int)((unsigned long long)wide >> 12);
    }
    *source_target_ratio = difference;
    if (input_diagonal == output_diagonal) {
        value = 0x8000000;
        *screen_ratio = value;
    } else {
        value = dovi_fxLOG2Taylor(input_diagonal, 26);
        other = dovi_fxLOG2Taylor(output_diagonal, 26);
        norm = dovi_unorm_s(other);
        dovi_fx_assert((int)(~norm >> 31), "sshl_su() called with negative shift value");
        other = dovi_sdiv_sls(0x2000000000000000LL, shl32(other, norm));
        shift = 34U - norm;
        dovi_fx_assert((int)(~shift >> 31), "sshr_slu() called with negative shift value");
        if ((int)shift > 63)
            shift = 63;
        wide = product_shift(other, value, shift);
        value = dovi_sshr_sss((int)dovi_ssqrt_s((unsigned int)sat32(wide)), 2);
        *screen_ratio = value;
    }
    value = dovi_smpy_ss(value, *source_target_ratio);
    value = dovi_sadd_sss(*screen_ratio, value);
    value = dovi_sshl_ssu(value, 3);
    other = dovi_smpy_ss(value, display->contrast);
    value = dovi_sadd_sss(value, other);
    *slope = value;
    wide = product_shift(sub32(mid, output_middle), key_weight, 17);
    dovi_fx_assert(1, "sshr_slu() called with negative shift value");
    value = dovi_sadd_sss(sat32(wide), 0x40000000);
    other = *source_target_ratio;
    wide = (long long)other * value;
    norm = dovi_unorm_sl(wide);
    shift = norm + 26U;
    value = *screen_ratio;
    dovi_fx_assert((int)(~shift >> 31), "sshr_slu() called with negative shift value");
    if ((int)shift > 63)
        shift = 63;
    wide = product_shift(high32(wide, norm), value, shift);
    offset = dovi_sadd_sss(sat32(wide), display->brightness);
    value = dovi_ssub_sss(crush, offset);
    if (input_min < value)
        input_min = dovi_ssub_sss(crush, offset);
    *target_min = dovi_sadd_sss(input_min, display->min_bias);
    value = dovi_ssub_sss(source_max, offset);
    if (value < input_max)
        input_max = dovi_ssub_sss(source_max, offset);
    *target_max = dovi_sadd_sss(input_max, display->max_bias);
    value = dovi_smpy_ss(sub32(source_max, crush), 0x6ccccccc);
    upper = dovi_sadd_sss(value, crush);
    value = dovi_smpy_ss(sub32(source_max, crush), 0x13333333);
    lower = dovi_sadd_sss(value, crush);
    if (mid < upper)
        upper = mid;
    if (upper < lower)
        upper = lower;
    *source_mid = upper;
    value = dovi_smpy_ss(sub32(*target_max, *target_min), 0x6ccccccc);
    upper = dovi_sadd_sss(value, *target_min);
    value = dovi_smpy_ss(sub32(*target_max, *target_min), 0x13333333);
    lower = dovi_sadd_sss(value, *target_min);
    value = dovi_sadd_sss(sub32(*source_mid, offset), display->mid_bias);
    if (upper < value)
        value = upper;
    if (value < lower)
        value = lower;
    *target_mid = value;
}

static void limit_pair(struct tone_curve_pair *pair)
{
    unsigned int shift = pair->norm;
    if (shift > 31U) {
        int value = pair->value;
        shift -= 31U;
        if (shift > 31U)
            shift = 31U;
        pair->norm = 31;
        pair->value = value >> shift;
    }
}

void calc_curve_params(int source_min, int source_mid, int source_max, int target_min,
                       int target_mid, int target_max, int slope,
                       struct tone_curve_display *display, struct tone_curve_pair *param1,
                       struct tone_curve_pair *param2, struct tone_curve_pair *param3,
                       int *slope_over_rolloff)
{
    int x1, x2, x3, y1, y2, y3, nx1, nx2, nx3, ny1, ny2, ny3;
    int xy12, xy13, xy23, numerator1, numerator2, numerator3, inverse, sor;
    int sign1 = 1, sign2 = 1, sign3 = 1;
    unsigned int xn1, xn2, xn3, yn1, yn2, yn3, xn12, xn13, xn23;
    unsigned int denom_norm, numerator_norm, product_norm, common;
    unsigned char ynx1, ynx3;
    long long wide, temp, denom;
    dovi_fx_assert(1, "sshl_su() called with negative shift value");
    dovi_fx_assert(1, "sshr_slu() called with negative shift value");
    wide = product_shift(slope, shl32(display->rolloff_inverse, 29), 31);
    sor = wide == 0x80000000LL ? 0x7fffffff : (int)wide;
    x1 = dovi_fxLOG2Taylor(source_min, 26);
    dovi_fx_assert(1, "sshl_su() called with negative shift value");
    x1 = dovi_fxPOW2Taylor(dovi_smpy_ss(add32(x1, -0x7c000000), sor), 23, 31);
    xn1 = dovi_unorm_s(x1);
    nx1 = shl32(x1, xn1);
    dovi_fx_assert((int)(~xn1 >> 31), "sshl_su() called with negative shift value");
    x2 = dovi_fxLOG2Taylor(source_mid, 26);
    dovi_fx_assert(1, "sshl_su() called with negative shift value");
    x2 = dovi_fxPOW2Taylor(dovi_smpy_ss(add32(x2, -0x7c000000), sor), 23, 31);
    xn2 = dovi_unorm_s(x2);
    nx2 = shl32(x2, xn2);
    dovi_fx_assert((int)(~xn2 >> 31), "sshl_su() called with negative shift value");
    x3 = dovi_fxLOG2Taylor(source_max, 26);
    dovi_fx_assert(1, "sshl_su() called with negative shift value");
    x3 = dovi_fxPOW2Taylor(dovi_smpy_ss(add32(x3, -0x7c000000), sor), 23, 31);
    xn3 = dovi_unorm_s(x3);
    nx3 = shl32(x3, xn3);
    dovi_fx_assert((int)(~xn3 >> 31), "sshl_su() called with negative shift value");
    if (target_min < 0) {
        target_min = neg32(target_min);
        sign1 = -1;
    }
    y1 = dovi_fxLOG2Taylor(target_min, 26);
    dovi_fx_assert(1, "sshl_su() called with negative shift value");
    inverse = display->rolloff_inverse;
    dovi_fx_assert(1, "sshl_su() called with negative shift value");
    y1 = dovi_fxPOW2Taylor(dovi_smpy_ss(add32(y1, -0x7c000000), shl32(inverse, 29)), 24, 31);
    if (sign1 < 0)
        y1 = neg32(y1);
    yn1 = dovi_unorm_s(y1);
    ny1 = shl32(y1, yn1);
    dovi_fx_assert((int)(~yn1 >> 31), "sshl_su() called with negative shift value");
    if (target_mid < 0) {
        target_mid = neg32(target_mid);
        sign2 = -1;
    }
    y2 = dovi_fxLOG2Taylor(target_mid, 26);
    dovi_fx_assert(1, "sshl_su() called with negative shift value");
    inverse = display->rolloff_inverse;
    dovi_fx_assert(1, "sshl_su() called with negative shift value");
    y2 = dovi_fxPOW2Taylor(dovi_smpy_ss(add32(y2, -0x7c000000), shl32(inverse, 29)), 24, 31);
    if (sign2 < 0)
        y2 = neg32(y2);
    yn2 = dovi_unorm_s(y2);
    ny2 = shl32(y2, yn2);
    dovi_fx_assert((int)(~yn2 >> 31), "sshl_su() called with negative shift value");
    if (target_max < 0) {
        target_max = neg32(target_max);
        sign3 = -1;
    }
    xn13 = xn1 + xn3;
    xn23 = xn2 + xn3;
    xn12 = xn1 + xn2;
    ynx1 = (unsigned char)(xn1 + yn1);
    y3 = dovi_fxLOG2Taylor(target_max, 26);
    dovi_fx_assert(1, "sshl_su() called with negative shift value");
    inverse = display->rolloff_inverse;
    dovi_fx_assert(1, "sshl_su() called with negative shift value");
    y3 = dovi_fxPOW2Taylor(dovi_smpy_ss(add32(y3, -0x7c000000), shl32(inverse, 29)), 24, 31);
    if (sign3 < 0)
        y3 = neg32(y3);
    yn3 = dovi_unorm_s(y3);
    ynx3 = (unsigned char)(xn3 + yn3);
    ny3 = shl32(y3, yn3);
    dovi_fx_assert((int)(~yn3 >> 31), "sshl_su() called with negative shift value");
    xy12 = dovi_smpy_ss(nx1, nx2);
    xy13 = dovi_smpy_ss(nx1, nx3);
    xy23 = dovi_smpy_ss(nx2, nx3);
    wide = add64(product_shift(xy23, y2, xn23), product_shift(xy13, y3, xn13));
    wide = sub64(wide, product_shift(y3, xy23, xn23));
    wide = sub64(wide, product_shift(y2, xy12, xn12));
    wide = add64(product_shift(xy12, y1, xn12), wide);
    denom = sub64(wide, product_shift(xy13, y1, xn13));
    denom_norm = dovi_unorm_sl(denom);
    inverse = dovi_sdiv_sls(0x2000000000000000LL, high32(denom, denom_norm));
    numerator1 = dovi_smpy_ss(xy23, ny1);
    numerator2 = dovi_ssub_sss(y2, y3);
    numerator3 = dovi_smpy_ss(xy13, ny2);
    temp = dovi_ssub_sss(y1, y3);
    xy12 = dovi_smpy_ss(xy12, ny3);
    xy23 = dovi_ssub_sss(y1, y2);
    wide = sub64(product_shift(numerator1, numerator2, yn1 + xn23),
                 product_shift(numerator3, (int)temp, yn2 + xn13));
    wide = add64(product_shift(xy12, xy23, yn3 + xn12), wide);
    numerator_norm = dovi_unorm_sl(wide);
    dovi_fx_assert(1, "sshr_slu() called with negative shift value");
    wide = mul64(inverse, shl64(wide, numerator_norm) >> 32);
    product_norm = dovi_unorm_sl(wide);
    param1->value = high32(wide, product_norm);
    param1->norm = (unsigned short)(numerator_norm + 29U - denom_norm + product_norm);
    numerator1 = dovi_smpy_ss(nx3, ny3);
    numerator2 = dovi_smpy_ss(nx2, ny2);
    numerator3 = dovi_smpy_ss(nx1, ny1);
    wide = sub64(product_shift(y1, numerator1, ynx3), product_shift(y1, numerator2, xn2 + yn2));
    wide = add64(wide, product_shift(y2, numerator3, ynx1));
    numerator1 = dovi_smpy_ss(nx3, ny3);
    wide = sub64(wide, product_shift(y2, numerator1, ynx3));
    numerator3 = dovi_smpy_ss(nx1, ny1);
    wide = sub64(wide, product_shift(y3, numerator3, ynx1));
    numerator2 = dovi_smpy_ss(nx2, ny2);
    wide = add64(wide, product_shift(y3, numerator2, xn2 + yn2));
    numerator_norm = dovi_unorm_sl(wide);
    wide = mul64(inverse, shl64(wide, numerator_norm) >> 32);
    dovi_fx_assert(1, "sshr_slu() called with negative shift value");
    product_norm = dovi_unorm_sl(wide);
    common = (unsigned short)(29U - denom_norm);
    param2->value = high32(wide, product_norm);
    wide = sub64((long long)y1 * sub32(x3, x2), (long long)y2 * sub32(x3, x1));
    wide = add64(wide, (long long)y3 * sub32(x2, x1));
    param2->norm = (unsigned short)(common + numerator_norm + product_norm);
    numerator_norm = dovi_unorm_sl(wide);
    wide = (long long)inverse * high32(wide, numerator_norm);
    product_norm = dovi_unorm_sl(wide);
    param3->value = high32(wide, product_norm);
    param3->norm = (unsigned short)(common + numerator_norm + product_norm);
    limit_pair(param1);
    limit_pair(param2);
    limit_pair(param3);
    *slope_over_rolloff = sor;
}
