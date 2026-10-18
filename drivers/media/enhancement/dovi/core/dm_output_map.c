/* Output color-space and transfer maps. */
#include "dm_output_map.h"
#include "numeric_helpers.h"
#include "fixed_bits.h"
#include "color_basis.h"
#include "matrix_arithmetic.h"
#include "color_narrow.h"
#include "color_primaries.h"
#include "numeric_color_matrix.h"
#include "control_abi_recovered.h"
#include "byte_utils.h"
#include "fixed_access.h"

static void identity(void *matrix)
{
    /* Machine stores zeroes and ones; it does not scale the identity. */
    unsigned int n;
    for (n = 0u; n < 9u; ++n)
        fixed_store_le16((unsigned char *)matrix + (n * 2u), (n % 4u) == 0u ? 1u : 0u);
}

int dovi_dm_CommitSigEnv2KsOMap(const void *config, int update_target_minmax, void *dm)
{
    const unsigned char *target = (const unsigned char *)config + DM_CONFIG_TGTSIGENV_OFFSET;
    unsigned char *kernel = (unsigned char *)(void *)fixed_load_le64((const unsigned char *)dm +
                                                                     (DM_CONTEXT_HDMKS_OFFSET));
    unsigned char *out = kernel + DM_KERNEL_KSOMAP_OFFSET;
    unsigned char *csc = (unsigned char *)dm + DM_CONTEXT_CSCCTRL_OFFSET;
    unsigned int color, bits, range, extent;
    short scale;
    int offsets[3];
    bits = fixed_load_le32((const unsigned char *)target + (36u));
    fixed_store_le32((unsigned char *)csc + (112u), 32767u);
    fixed_store_le32((unsigned char *)csc + (108u), 32767u);
    fixed_store_le32((unsigned char *)csc + (104u), 32767u);
    fixed_store_le32((unsigned char *)csc + (116u), 15u);
    color = fixed_load_le32((const unsigned char *)target + (16u));
    out[60] = color == 1u ? 1u : color == 2u ? 2u : 0u;
    dovi_AssignM33I2I(csc + 144u, out + 2u);
    fixed_store_le16((unsigned char *)out + (0u),
                     fixed_load_le32((const unsigned char *)csc + (164u)));
    if (color == 3u && fixed_load_le32((const unsigned char *)target + (156u)) == 0u) {
        identity(out + 22u);
        fixed_store_le16((unsigned char *)out + (20u), 0u);
    } else {
        if (fixed_load_le32((const unsigned char *)target + (156u)) != 0u) {
            dovi_MemCpyByte(csc + 80u, target + 160u, 18);
            fixed_store_le32((unsigned char *)csc + (100u),
                             fixed_load_le32((const unsigned char *)target + (180u)));
        } else if (fixed_load_le32((const unsigned char *)target + (120u)) == 0u) {
            scale = dovi_GetLms2RgbByDefM33Fxp(
                fixed_load_le32((const unsigned char *)target + (48u)), csc + 80u);
            fixed_store_le32((unsigned char *)csc + (100u), (unsigned int)(int)scale);
        } else {
            scale = dovi_GetLms2RgbByPrimsM33Fxp(
                (int)fixed_load_le32((const unsigned char *)target + (124u)),
                (int)fixed_load_le32((const unsigned char *)target + (128u)),
                (int)fixed_load_le32((const unsigned char *)target + (132u)),
                (int)fixed_load_le32((const unsigned char *)target + (136u)),
                (int)fixed_load_le32((const unsigned char *)target + (140u)),
                (int)fixed_load_le32((const unsigned char *)target + (144u)),
                (int)fixed_load_le32((const unsigned char *)target + (148u)),
                (int)fixed_load_le32((const unsigned char *)target + (152u)), csc + 80u);
            fixed_store_le32((unsigned char *)csc + (100u), (unsigned int)(int)scale);
        }
        if (fixed_load_le32((const unsigned char *)target + (52u)) != 0u) {
            dovi_MemCpyByte(csc + 104u, target + 56u, 12);
            fixed_store_le32(
                (unsigned char *)csc + (116u),
                (unsigned int)(int)(short)fixed_load_le16((const unsigned char *)target + (68u)));
        }
        scale = dovi_GetLms2RgbCtWpM33Fxp(
            csc + 80u, (short)fixed_load_le32((const unsigned char *)csc + (100u)),
            (int)fixed_load_le32((const unsigned char *)target + (200u)), csc + 104u,
            (short)fixed_load_le32((const unsigned char *)csc + (116u)), out + 22u);
        fixed_store_le16((unsigned char *)out + (20u), (unsigned short)scale);
    }
    if (color == 3u) {
        fixed_store_le32((unsigned char *)out + (40u), 0u);
        fixed_store_le32((unsigned char *)out + (44u), 0x9c400000u);
    }
    if (update_target_minmax != 0) {
        unsigned char *current = (unsigned char *)(void *)fixed_load_le64(
                                     (const unsigned char *)dm + (DM_CONTEXT_HDMKS_OFFSET)) +
                                 DM_KERNEL_KSOMAP_OFFSET;
        unsigned int minimum = fixed_load_le32((const unsigned char *)target + (184u)),
                     maximum = fixed_load_le32((const unsigned char *)target + (188u));
        if (fixed_load_le32((const unsigned char *)target + (40u)) == 1u ||
            fixed_load_le32((const unsigned char *)target + (16u)) == 3u) {
            minimum = 0u;
            maximum = 0x9c400000u;
        }
        fixed_store_le32((unsigned char *)current + (40u), minimum);
        fixed_store_le32((unsigned char *)current + (44u), maximum);
    }
    if (color == 3u) {
        fixed_store_le16((unsigned char *)out + (48u), 0u);
        fixed_store_le16((unsigned char *)out + (56u), bits);
        out[58] = 1u;
        out[60] = 0u;
        extent = fixed_load_le32((const unsigned char *)target + (36u));
        fixed_store_le32((unsigned char *)out + (52u),
                         kernel_shift_left(kernel_shift_left(1u, extent) - 1u, 16u - extent));
        if (fixed_load_le32((const unsigned char *)target + (76u)) == 0u) {
            scale =
                fixed_load_le32((const unsigned char *)target + (44u)) == 1u
                    ? dovi_GetLms2IptM33Fxp(out + 64u)
                    : dovi_GetLms2IptM33NarrowFxp(
                          (short)fixed_load_le32((const unsigned char *)target + (36u)), out + 64u);
            fixed_store_le16((unsigned char *)out + (62u), (unsigned short)scale);
        } else {
            dovi_AssignM33I2I(target + 80u, out + 64u);
            fixed_store_le16((unsigned char *)out + (62u),
                             fixed_load_le32((const unsigned char *)target + (100u)));
        }
    } else {
        unsigned short minimum, length;
        range = (fixed_load_le32((const unsigned char *)target + (16u)) - 1u) < 2u
                    ? fixed_load_le32((const unsigned char *)target + (44u))
                    : 1u;
        dovi_GetRange((int)bits, range, &minimum, &length);
        fixed_store_le16((unsigned char *)out + (48u), minimum);
        fixed_store_le16((unsigned char *)out + (50u), length);
        extent = kernel_shift_left(1u, fixed_load_le32((const unsigned char *)target + (36u))) - 1u;
        fixed_store_le32((unsigned char *)out + (52u), extent
                                                           ? (unsigned int)length * 65536u / extent
                                                           : (unsigned int)length * 65536u);
        fixed_store_le16((unsigned char *)out + (56u), bits);
        out[58] = (unsigned char)fixed_load_le32((const unsigned char *)target + (40u));
        if (fixed_load_le32((const unsigned char *)target + (76u)) == 0u) {
            scale =
                fixed_load_le32((const unsigned char *)target + (44u)) == 0u
                    ? dovi_GetRgb2YuvM33NarrowFxp(
                          fixed_load_le32((const unsigned char *)target + (72u)),
                          (int)fixed_load_le32((const unsigned char *)target + (36u)), out + 64u)
                    : dovi_GetRgb2YuvM33Fxp(fixed_load_le32((const unsigned char *)target + (72u)),
                                            out + 64u);
            fixed_store_le16((unsigned char *)out + (62u), (unsigned short)scale);
        } else {
            dovi_AssignM33I2I(target + 80u, out + 64u);
            fixed_store_le16((unsigned char *)out + (62u),
                             fixed_load_le32((const unsigned char *)target + (100u)));
        }
    }
    if (fixed_load_le32((const unsigned char *)target + (104u)) != 0u) {
        dovi_AssignV3I2I(target + 108u, offsets);
        fixed_store_le32((unsigned char *)out + (88u),
                         (unsigned int)offsets[1] >> ((15u - bits) & 31u));
        fixed_store_le32((unsigned char *)out + (84u),
                         (unsigned int)offsets[0] >> ((15u - bits) & 31u));
        fixed_store_le32((unsigned char *)out + (92u),
                         (unsigned int)offsets[2] >> ((15u - bits) & 31u));
    } else if (color == 3u)
        dovi_GetYuvRgbOffFxp(fixed_load_le32((const unsigned char *)target + (44u)),
                             (int)fixed_load_le32((const unsigned char *)target + (36u)),
                             out + 84u);
    else {
        dovi_GetYuvRgbOffFxp(fixed_load_le32((const unsigned char *)target + (44u)), (int)bits,
                             offsets);
        fixed_store_le32((unsigned char *)out + (84u), (unsigned int)offsets[0]);
        fixed_store_le32((unsigned char *)out + (88u), (unsigned int)offsets[1]);
        fixed_store_le32((unsigned char *)out + (92u), (unsigned int)offsets[2]);
    }
    range = fixed_load_le32((const unsigned char *)target + (44u));
    extent = fixed_load_le32((const unsigned char *)target + (36u));
    dovi_GetYuvRgbOffFxp(range, (int)extent, out + 100u);
    fixed_store_le32((unsigned char *)out + (96u),
                     range == 0u ? ((int)extent > 7 ? kernel_shift_left(219u, extent - 8u) : 219u)
                                 : kernel_shift_left(1u, extent) - 1u);
    return 0;
}
