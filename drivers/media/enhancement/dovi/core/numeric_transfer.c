/* Fixed-point transfer functions and gamma lookup-table generation. */
#include "numeric_transfer.h"
#include "fixed_arithmetic.h"
#include "fixed_transcendentals.h"
#include "pq_conversion.h"
#include "numeric_tables.h"

void dovi_Cct2Xy(unsigned long long cct, void *out)
{
    unsigned long long t = fixed_asr64(cct, 26);
    unsigned int a, b, x, y, z, den;
    a = (unsigned int)dovi_sdiv(t * 0x88fe2c1, 0x1968);
    b = (unsigned int)dovi_sdiv(t * 0x275b09c1, 0x1968);
    x = (unsigned int)dovi_sdiv(fixed_sign32(a + 0x5e3cb2a) << 26, b + 0x4000000);
    a = (unsigned int)dovi_sdiv(t * 0x19d0cac, 0x1968);
    b = (unsigned int)dovi_sdiv(t * 0x6e989b5, 0x1968);
    y = (unsigned int)dovi_sdiv(fixed_sign32(a + 0x206213b) << 26, b + 0x4000000);
    z = 0x4000000 - x - y;
    den = (unsigned int)((fixed_sign32(y) * 0x3fffffff4e11b59ull + fixed_sign32(x) * 0x26298c43 +
                          fixed_sign32(z) * 0x1597317c) >>
                         26);
    fixed_store_le32(out, (unsigned int)dovi_sdiv(fixed_sign32(x) * 0x211f57ee -
                                                      fixed_sign32(y) * 0x1538b7fc +
                                                      fixed_sign32(z) * 0x34e7154,
                                                  den));
    fixed_store_le32((unsigned char *)out + 4,
                     (unsigned int)dovi_sdiv(fixed_sign32(x) * 0x5d5f80f +
                                                 fixed_sign32(y) * 0xae39933 -
                                                 fixed_sign32(z) * 0xb99142,
                                             den));
}

void dovi_Xy2Xyz(const void *input, void *output)
{
    const unsigned char *in = input;
    unsigned char *out = output;
    unsigned int z;
    if (fixed_less32(fixed_load_le32(in + 4), 0x4000)) {
        fixed_store_le32(out + 4, fixed_load_le32(in + 4));
        fixed_store_le32(out, fixed_load_le32(in));
        z = 0x4000000 - fixed_load_le32(in) - fixed_load_le32(in + 4);
    } else {
        fixed_store_le32(out + 4, 0x4000000);
        fixed_store_le32(out, (unsigned int)dovi_sdiv(fixed_sign32(fixed_load_le32(in)) << 26,
                                                      fixed_load_le32(in + 4)));
        z = (unsigned int)dovi_sdiv(0x10000000000000ull - (fixed_sign32(fixed_load_le32(in + 4)) +
                                                           fixed_sign32(fixed_load_le32(in))) *
                                                              0x4000000,
                                    fixed_load_le32(in + 4));
    }
    fixed_store_le32(out + 8, z);
}

unsigned int dovi_Xyz2IctcpDm_m33Xyz2Lms[9], dovi_Xyz2IctcpDm_v3_0, dovi_Xyz2IctcpDm_v3_1;

void dovi_Xyz2IctcpDm(const void *input, void *output)
{
    const unsigned char *in = input;
    unsigned char *out = output;
    unsigned long long l, m, s, a, b, c;
    unsigned int p, q, r, i;
    static const unsigned int basis[9]={0x6676031, 0xb524d33, 0xfeb50535, 0xfc61153e, 0x12a521f6, 0xbb30bc, 0, 0, 0xeb10ca5};
    for (i = 0; i < 9; i++)
        dovi_Xyz2IctcpDm_m33Xyz2Lms[i] = basis[i];
    l = fixed_asr64(fixed_load_le64(in) * 0x6676031 + fixed_load_le64(in + 8) * 0xb524d33 -
                        fixed_load_le64(in + 16) * 0x14afacb,
                    28);
    m = fixed_asr64(0ull - fixed_load_le64(in) * 0x39eeac2 + fixed_load_le64(in + 8) * 0x12a521f6 +
                        fixed_load_le64(in + 16) * 0xbb30bc,
                    28);
    s = fixed_asr64(fixed_load_le64(in + 16) * 0xeb10ca5, 28);
    a = l * 0x3ae16 + (m + s) * 0x28f5;
    b = m * 0x3ae16 + (l + s) * 0x28f5;
    c = s * 0x3ae16 + (l + m) * 0x28f5;
    p = (unsigned int)dovi_L2PqX(a >> 63 ? 0 : (a >> 12) & 0xfffffffffffc0ull);
    dovi_Xyz2IctcpDm_v3_0 = p;
    q = (unsigned int)dovi_L2PqX(b >> 63 ? 0 : (b >> 12) & 0xfffffffffffc0ull);
    dovi_Xyz2IctcpDm_v3_1 = q;
    r = (unsigned int)dovi_L2PqX(c >> 63 ? 0 : (c >> 12) & 0xfffffffffffc0ull);
    fixed_store_le32(out, (unsigned int)((fixed_sign32(q) + fixed_sign32(p)) >> 1));
    fixed_store_le32(out + 4, (unsigned int)((fixed_sign32(p) * 0xce9 + fixed_sign32(r) * 0xdad +
                                              fixed_sign32(q) * 0xfffffffe56aull) >>
                                             12));
    fixed_store_le32(out + 8,
                     (unsigned int)((fixed_sign32(r) * 0xffffffffde1ull + fixed_sign32(p) * 0x460d +
                                     fixed_sign32(q) * 0xfffffffbc12ull) >>
                                    12));
}

void dovi_RgbLn2Ictcp(unsigned int red, unsigned int green, unsigned int blue, const void *matrix,
                      unsigned long long scale, const void *matrix2, unsigned int scale2,
                      void *output)
{
    const unsigned char *a = matrix, *b = matrix2;
    unsigned char *out = output;
    unsigned long long x[3];
    unsigned int r, c, p[3], rgb[3] = {red, green, blue};
    for (r = 0; r < 3; r++) {
        unsigned long long sum = 0;
        for (c = 0; c < 3; c++)
            sum += fixed_sign32(fixed_sign16(fixed_load_le16(a + (r * 3 + c) * 2))) * rgb[c];
        x[r] = fixed_asr64(sum, (unsigned int)scale);
        if (x[r] >> 63)
            x[r] = 0;
    }
    for (r = 0; r < 3; r++)
        p[r] = (unsigned int)dovi_L2PqX(x[r] << 6);
    for (r = 0; r < 3; r++) {
        unsigned long long sum = 0;
        for (c = 0; c < 3; c++)
            sum +=
                numeric_multiply_signed32(fixed_sign16(fixed_load_le16(b + (r * 3 + c) * 2)), p[c]);
        fixed_store_le32(out + r * 4, (unsigned int)fixed_asr64(sum, scale2));
    }
}

unsigned long long dovi_GetIntendedWpCct(unsigned int source, unsigned int target,
                                         unsigned int index)
{
    index &= 255;
    source &= 255;
    target &= 255;
    if (!index) {
        if (target >= 6)
            target = 0;
        if (target == 5) {
            __asm__ volatile("brk #0x5512");
            __builtin_unreachable();
        }
        if (source >= 6)
            source = 0;
        index = ((const unsigned char *)numeric_wp_cct)[source * 5 + target];
    }
    return (unsigned long long)index * 0x5dc000000ull + 0x65a0000000ull;
}

void dovi_GetGammaABG(unsigned int gamma, unsigned long long maximum, unsigned long long minimum,
                      void *alpha, void *beta, void *outgamma)
{
    unsigned int shift = dovi_unorm_s(gamma << 1),
                 normal = shift >> 31 ? 0 : (gamma << 1) << (shift & 31);
    unsigned int inverse = (unsigned int)dovi_sdiv_sls(0x2000000000000000ull, normal), a, b, delta,
                 tmp;
    a = dovi_fxPOW2Taylor(
        (unsigned int)dovi_smpy_ss(
            inverse, dovi_fxLOG2Taylor((unsigned int)(maximum >> 4) & 0xfffffff, 26) + 0xc8000000),
        41 - shift, 17);
    b = dovi_fxPOW2Taylor(
        (unsigned int)dovi_smpy_ss(
            inverse, dovi_fxLOG2Taylor((unsigned int)(minimum >> 4) & 0xfffffff, 26) + 0xc8000000),
        41 - shift, 17);
    delta = a - b;
    tmp = dovi_fxPOW2Taylor(
        (unsigned int)dovi_smpy_ss(normal, dovi_fxLOG2Taylor(delta, 26) + 0xbc000000), shift + 10,
        17);
    fixed_store_le32(alpha, tmp);
    shift = dovi_unorm_s(delta);
    normal = shift >> 31 ? 0 : delta << (shift & 31);
    tmp = (unsigned int)dovi_sdiv_sls(0x2000000000000000ull, normal);
    tmp = (unsigned int)dovi_smpy_ss(tmp, b);
    tmp = dovi_sshr_sss(tmp, 5 - shift);
    fixed_store_le32(beta, tmp);
    fixed_store_le32(alpha, fixed_asr32(fixed_load_le32(alpha) + 0x4000, 15));
    fixed_store_le32(beta, fixed_asr32(fixed_load_le32(beta) + 0x100, 9));
    fixed_store_le32(outgamma, dovi_fxPOW2Taylor(gamma, 14, 18));
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
            fixed_store_le64(sl + i * 8u,
                             fixed_raw_divide64((unsigned long long)dy << 31, fixed_sign32(dx)));
        }
    }
}
