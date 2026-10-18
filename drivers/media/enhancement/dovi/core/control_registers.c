/* Encode the composer and display-management hardware payloads. */
#include "control_registers.h"
#include "fixed_access.h"
#include "composer_contract.h"

typedef char require_u32[(sizeof(unsigned int) == 4) ? 1 : -1];
typedef char require_u64[(sizeof(unsigned long long) == 8) ? 1 : -1];

static unsigned int read32(const unsigned char *p)
{
    return fixed_load_le32(p);
}

static void write32(unsigned char *p, unsigned int word)
{
    fixed_store_le32(p, word);
}

static void copy32(unsigned char *out, const unsigned char *in)
{
    write32(out, read32(in));
}

static void polynomial(const unsigned char *in, unsigned char *out, unsigned int channel,
                       unsigned int offset, unsigned int rows)
{
    unsigned int i;
    for (i = 0; i < rows * 3u; ++i) {
        unsigned int at = channel * 96u + i * 4u;
        unsigned int shift = read32(in + COMPOSER_CONFIG_COEFFICIENT_LOG2_DENOM_OFFSET) & 31u;
        unsigned int word = (read32(in + COMPOSER_CONFIG_POLY_COEF_INT_OFFSET + at) << shift) |
                            read32(in + COMPOSER_CONFIG_POLY_COEF_OFFSET + at);
        write32(out + offset + i * 4u, word & 0x3fffffffu);
    }
}

static void non_linear(const unsigned char *in, unsigned char *out, unsigned int channel,
                       unsigned int offset)
{
    unsigned int i;
    copy32(out + offset, in + COMPOSER_CONFIG_NLQ_OFFSET_OFFSET + channel * 4u);
    for (i = 0; i < 3u; ++i) {
        unsigned int at = channel * 12u + i * 4u;
        unsigned int shift = read32(in + COMPOSER_CONFIG_COEFFICIENT_LOG2_DENOM_OFFSET) & 31u;
        unsigned int word = (read32(in + COMPOSER_CONFIG_NLQ_COEFF_INT_OFFSET + at) << shift) |
                            read32(in + COMPOSER_CONFIG_NLQ_COEFF_OFFSET + at);
        write32(out + offset + 4u + i * 4u, word & 0xffffffu);
    }
}

static void mmr(const unsigned char *in, unsigned char *out, unsigned int channel,
                unsigned int offset)
{
    unsigned int i;
    for (i = 0; i < 22u; ++i) {
        unsigned int at = (channel - 1u) * 88u + i * 4u;
        unsigned int integer = read32(in + COMPOSER_CONFIG_MMR_COEF_INT_OFFSET + at);
        unsigned long long wide = (unsigned long long)integer;
        unsigned int shift = read32(in + COMPOSER_CONFIG_COEFFICIENT_LOG2_DENOM_OFFSET) & 63u;
        if (integer & 0x80000000u)
            wide |= 0xffffffff00000000ull;
        wide = (wide << shift) | read32(in + COMPOSER_CONFIG_MMR_COEF_OFFSET + at);
        write32(out + offset + i * 8u, (unsigned int)wide);
        write32(out + offset + i * 8u + 4u, (unsigned int)(wide >> 32) & 255u);
    }
}

static void chroma(const unsigned char *in, unsigned char *out, unsigned int channel,
                   unsigned int mapping, unsigned int pivots, unsigned int order, unsigned int poly,
                   unsigned int mmr_at, unsigned int mmr_order, unsigned int nlq)
{
    unsigned int i, word = 0;
    copy32(out + mapping, in + COMPOSER_CONFIG_MAPPING_IDC_OFFSET + channel * 4u);
    copy32(out + pivots, in + COMPOSER_CONFIG_NUM_PIVOTS_OFFSET + channel * 4u);
    for (i = 0; i < 2u; ++i) {
        const unsigned char *p = in + COMPOSER_CONFIG_PIVOT_VALUE_OFFSET + channel * 36u + i * 8u;
        write32(out + pivots + 4u + i * 4u, read32(p) | (read32(p + 4) << 10));
    }
    copy32(out + pivots + 12u, in + COMPOSER_CONFIG_PIVOT_VALUE_OFFSET + channel * 36u + 16u);
    for (i = 0; i < 4u; ++i)
        word |= read32(in + COMPOSER_CONFIG_POLY_ORDER_OFFSET + channel * 32u + i * 4u) << (i * 2u);
    write32(out + order, word);
    polynomial(in, out, channel, poly, 4u);
    mmr(in, out, channel, mmr_at);
    copy32(out + mmr_order, in + COMPOSER_CONFIG_MMR_ORDER_OFFSET + (channel - 1u) * 4u);
    non_linear(in, out, channel, nlq);
}

static void encode_composer(const void *composer, void *registers, unsigned int width,
                            unsigned int height)
{
    const unsigned char *in = (const unsigned char *)composer;
    unsigned char *out = (unsigned char *)registers;
    unsigned int i, word = 0;
    write32(out + COMPOSER_REGISTER_COMPOSER_MODE_OFFSET,
            DOVI_COMPOSER_MODE_FROM_RESIDUAL ?
                in[COMPOSER_CONFIG_DISABLE_RESIDUAL_FLAG_OFFSET] != 0 : 0);
    write32(out + COMPOSER_REGISTER_VDR_RESOLUTION_OFFSET,
            (unsigned int)width | ((unsigned int)height << 13));
    write32(out + COMPOSER_REGISTER_BIT_DEPTH_OFFSET,
            (read32(in + COMPOSER_CONFIG_RPU_EL_BIT_DEPTH_OFFSET) << 4) |
                (read32(in + COMPOSER_CONFIG_RPU_VDR_BIT_DEPTH_OFFSET) << 8) |
                read32(in + COMPOSER_CONFIG_RPU_BL_BIT_DEPTH_OFFSET));
    copy32(out + COMPOSER_REGISTER_COEFFICIENT_LOG2_DENOMINATOR_OFFSET,
           in + COMPOSER_CONFIG_COEFFICIENT_LOG2_DENOM_OFFSET);
    copy32(out + COMPOSER_REGISTER_BL_NUM_PIVOTS_Y_OFFSET, in + COMPOSER_CONFIG_NUM_PIVOTS_OFFSET);
    for (i = 0; i < 4u; ++i) {
        const unsigned char *p = in + COMPOSER_CONFIG_PIVOT_VALUE_OFFSET + i * 8u;
        write32(out + COMPOSER_REGISTER_BL_PIVOT_OFFSET + i * 4u,
                read32(p) | (read32(p + 4) << 10));
    }
    copy32(out + COMPOSER_REGISTER_BL_PIVOT_OFFSET + 16u,
           in + COMPOSER_CONFIG_PIVOT_VALUE_OFFSET + 32u);
    for (i = 0; i < 8u; ++i)
        word |= read32(in + COMPOSER_CONFIG_POLY_ORDER_OFFSET + i * 4u) << (i * 2u);
    write32(out + COMPOSER_REGISTER_BL_ORDER_OFFSET, word);
    polynomial(in, out, 0, COMPOSER_REGISTER_BL_COEFFICIENT_Y_OFFSET, 8u);
    non_linear(in, out, 0, COMPOSER_REGISTER_EL_NLQ_OFFSET_Y_OFFSET);
    chroma(in, out, 1u, COMPOSER_REGISTER_MAPPING_IDC_U_OFFSET,
           COMPOSER_REGISTER_BL_NUM_PIVOTS_U_OFFSET, COMPOSER_REGISTER_BL_ORDER_U_OFFSET,
           COMPOSER_REGISTER_BL_COEFFICIENT_U_OFFSET, COMPOSER_REGISTER_MMR_COEFFICIENT_U_OFFSET,
           COMPOSER_REGISTER_MMR_ORDER_U_OFFSET, COMPOSER_REGISTER_EL_NLQ_OFFSET_U_OFFSET);
    chroma(in, out, 2u, COMPOSER_REGISTER_MAPPING_IDC_V_OFFSET,
           COMPOSER_REGISTER_BL_NUM_PIVOTS_V_OFFSET, COMPOSER_REGISTER_BL_ORDER_V_OFFSET,
           COMPOSER_REGISTER_BL_COEFFICIENT_V_OFFSET, COMPOSER_REGISTER_MMR_COEFFICIENT_V_OFFSET,
           COMPOSER_REGISTER_MMR_ORDER_V_OFFSET, COMPOSER_REGISTER_EL_NLQ_OFFSET_V_OFFSET);
}

unsigned short dovi_encode_composer_registers(const void *composer, void *registers,
                                              unsigned int width, unsigned int height)
{
    encode_composer(composer, registers, width, height);
    return 0;
}

void dovi_encode_display_luts(void *out, const void *tmap, const void *imap)
{
    static const unsigned int starts[4] = {4u, 1028u, 2056u, 3080u};
    unsigned int a, i, n;
    for (a = 0u; a < 4u; ++a)
        for (i = 0u; i < 256u; ++i) {
            n = starts[a] + i * 4u;
            fixed_store_le32((unsigned char *)out + a * 1024u + i * 4u,
                             (fixed_load_le16((const unsigned char *)tmap + n + 2u) & 0x7fffu)
                                     << 15 |
                                 (fixed_load_le16((const unsigned char *)tmap + n) & 0x7fffu));
        }
    for (i = 0u; i < 256u; ++i)
        fixed_store_le32((unsigned char *)out + 4096u + i * 4u,
                         fixed_load_le32((const unsigned char *)imap + 48u + i * 4u));
}
