/* Fixed-point transfer functions and gamma lookup-table generation. */
#include "numeric_transfer.h"
#include "numeric_transfer_tables.h"
#include "fixed_access.h"
#include "fixed_arithmetic.h"
#include "fixed_transcendentals.h"

int dovi_applyGamma(int linear, int inverse_gamma)
{
    long long product = (long long)linear * inverse_gamma;
    unsigned int norm = dovi_unorm_sl(product), scale = norm + 10u;
    int input = fixed_signed32((unsigned int)(product >> ((32u - norm) & 63u)));
    if (fixed_signed32(scale) > 31) {
        scale = 31;
        input >>= ((norm - 21u) & 31u);
    }
    return dovi_fxPOW2Taylor(input, fixed_signed32(scale), 27);
}

unsigned int dovi_L2PQ_loglut(unsigned int linear)
{
    unsigned int norm, index;
    unsigned long long product;
    if (linear < 256u)
        linear = 256u;
    norm = dovi_unorm_u_lut(linear);
    index = transfer_l2pq_8space_8b_normlut[(linear << (norm & 31u)) >> 24] - norm * 8u;
    product = (unsigned long long)(linear - transfer_log2_frac8_fxd[index]) *
              transfer_l2pq_slope_frac8[index];
    return transfer_l2pq_lut_frac8[index] + (unsigned int)(product >> 17);
}

int dovi_computeTgtEotfLut(void *lut, unsigned int maximum, unsigned int minimum,
                           unsigned int gamma, unsigned int eotf)
{
    unsigned char *output = lut;
    unsigned int inverse = gamma ? 0x7fffffffu / gamma : 0, i;
    if (maximum >> 31)
        maximum = 0x7fffffffu;
    if (!eotf) {
        int alpha, beta, logalpha;
        calc_ta_tb(fixed_signed32(gamma * 2u), maximum, minimum, &alpha, &beta);
        beta = dovi_sshr_sss(beta, -2);
        logalpha = dovi_fxLOG2Taylor(alpha, 25);
        for (i = 0; i < 97; ++i) {
            int value = dovi_ssub_sss(fixed_signed32(transfer_lin_rng_frac4[i]),
                                      fixed_signed32((unsigned int)logalpha - 0x22000000u));
            value = dovi_applyGamma(value, fixed_signed32(inverse));
            value = fixed_signed32((unsigned int)value - (unsigned int)beta);
            fixed_store_le32(output + i * 8u, value < 0 ? 0u : (unsigned int)value);
        }
    } else if (eotf == 1) {
        for (i = 0; i < 97; ++i)
            fixed_store_le32(output + i * 8u,
                             (dovi_L2PQ_loglut(transfer_log2_frac4_fxd[i]) + 8u) >> 4);
    } else {
        unsigned int logspan =
            (unsigned int)dovi_fxLOG2Taylor(fixed_signed32(maximum - minimum), 25);
        for (i = 0; i < 97; ++i) {
            unsigned int delta = transfer_log2_frac4_fxd[i] - minimum, value = 0;
            if (fixed_signed32(delta) > 0) {
                unsigned int logvalue = (unsigned int)dovi_fxLOG2Taylor(fixed_signed32(delta), 25);
                int input = dovi_ssub_sss(fixed_signed32(logvalue - 0x24000000u),
                                          fixed_signed32(logspan - 0x24000000u));
                value = (unsigned int)dovi_applyGamma(input, fixed_signed32(inverse));
            }
            fixed_store_le32(output + i * 8u, value);
        }
    }
    for (i = 0; i < 96; ++i) {
        unsigned int delta =
            fixed_load_le32(output + (i + 1u) * 8u) - fixed_load_le32(output + i * 8u);
        unsigned long long product =
            (unsigned long long)((long long)fixed_signed32(delta) *
                                 fixed_signed32(transfer_xdiffinv_frac4_fxd[i])) +
            0x20000ull;
        fixed_store_le32(output + i * 8u + 4u, (unsigned int)(fixed_signed64(product) >> 18));
    }
    return 0;
}

int dovi_l2eotf_lerp(unsigned int linear, const void *lut, unsigned int minimum,
                     unsigned int maximum)
{
    const unsigned char *input = lut;
    unsigned int norm, index, base;
    unsigned long long product;
    (void)maximum;
    if (linear < 256u)
        linear = 256u;
    if (linear <= minimum + 2u)
        return 0;
    norm = dovi_unorm_u_lut(linear);
    index = transfer_l2g_4space_8b_normlut[(linear << (norm & 31u)) >> 24] - norm * 4u;
    base = fixed_load_le32(input + index * 8u);
    product =
        (unsigned long long)(long long)fixed_signed32(fixed_load_le32(input + index * 8u + 4u)) *
        (linear - transfer_log2_frac4_fxd[index]);
    return fixed_signed32(base + (unsigned int)(product >> 18));
}

void dovi_GenerateL2GLut(unsigned int gamma, void *slopes, void *lut16, void *positions,
                         void *scratchx, void *scratchy)
{
    static const unsigned int breaks[6] = {
#include "gamma_lut_breaks.inc"
    };
    unsigned char *x = scratchx, *y = scratchy, *lut = lut16, *pos = positions, *sl = slopes;
    unsigned int factor =
        (unsigned int)dovi_fxPOW2Taylor(fixed_signed32((unsigned int)gamma), 31, 30);
    unsigned int prev = 0, next = 1, count = 1, section = 0, division = 0, step = 0, level, i;
    fixed_store_le32(x, 16);
    fixed_store_le32(y, (unsigned int)dovi_powTaylor(16, (unsigned int)gamma));
    for (level = 26; level; --level) {
        unsigned int done = 0, split = breaks[section + 1u];
        if (level == split) {
            count <<= 1;
            step = fixed_load_le32(x + prev * 4u) >> (division & 31u);
            ++section;
            ++division;
        }
        while (done < count) {
            fixed_store_le32(x + next * 4u, fixed_load_le32(x + prev * 4u) << 1);
            fixed_store_le32(
                y + next * 4u,
                (unsigned int)(((unsigned long long)fixed_load_le32(y + prev * 4u) * factor) >>
                               30));
            ++next;
            if (level == split) {
                unsigned int value = fixed_load_le32(x + (next - 1u) * 4u) + step;
                fixed_store_le32(x + next * 4u, value);
                fixed_store_le32(y + next * 4u,
                                 (unsigned int)dovi_powTaylor(value, (unsigned int)gamma));
                ++next;
                done += 2;
            } else
                ++done;
            ++prev;
        }
        /* Immutable breakpoints keep the previous section index synchronized. */
    }
    for (i = next; i < 129; ++i) {
        fixed_store_le32(x + i * 4u, 0x7fffffffu);
        fixed_store_le32(y + i * 4u, 0x7fffffffu);
    }
    if (!sl) {
        for (i = 0; i < 129; ++i) {
            unsigned int value = (fixed_load_le32(y + i * 4u) + 0x4000u) >> 15;
            fixed_store_le16(lut + i * 2u, value > 0xffffu ? 0xffffu : value);
        }
    } else {
        for (i = 0; i < 128; ++i) {
            unsigned int value, dx, dy;
            fixed_store_le32(pos + i * 4u, fixed_load_le32(x + i * 4u));
            value = (fixed_load_le32(y + i * 4u) + 0x4000u) >> 15;
            fixed_store_le16(lut + i * 2u, value > 0xffffu ? 0xffffu : value);
            dx = fixed_load_le32(x + (i + 1u) * 4u) - fixed_load_le32(x + i * 4u);
            dy = fixed_load_le32(y + (i + 1u) * 4u) - fixed_load_le32(y + i * 4u);
            fixed_store_le64(sl + i * 8u, dx ? ((unsigned long long)dy << 31) / dx : 0);
        }
    }
}

void calc_ta_tb(int gamma, unsigned int maximum, unsigned int minimum, int *ta, int *tb)
{
    unsigned int norm = dovi_unorm_s(gamma), diff_norm;
    int normalized = fixed_signed32((unsigned int)gamma << (norm & 31u));
    int inverse = dovi_sdiv_sls(0x2000000000000000ll, normalized);
    int high = dovi_fxLOG2Taylor((int)(maximum >> 4), 26);
    int low, difference, result;
    high = dovi_smpy_ss(inverse, fixed_signed32((unsigned int)high - 0x38000000u));
    high = dovi_fxPOW2Taylor(high, fixed_signed32(41u - norm), 17);
    low = dovi_fxLOG2Taylor((int)(minimum >> 4), 26);
    low = dovi_smpy_ss(inverse, fixed_signed32((unsigned int)low - 0x38000000u));
    low = dovi_fxPOW2Taylor(low, fixed_signed32(41u - norm), 17);
    difference = fixed_signed32((unsigned int)high - (unsigned int)low);
    result = dovi_fxLOG2Taylor(difference, 26);
    result = dovi_smpy_ss(normalized, fixed_signed32((unsigned int)result - 0x44000000u));
    result = dovi_fxPOW2Taylor(result, fixed_signed32(norm + 10u), 17);
    *ta = result;
    diff_norm = dovi_unorm_s(difference);
    inverse = dovi_sdiv_sls(0x2000000000000000ll,
                            fixed_signed32((unsigned int)difference << (diff_norm & 31u)));
    result = dovi_smpy_ss(inverse, low);
    *tb = dovi_sshr_sss(result, fixed_signed32(5u - diff_norm));
}
