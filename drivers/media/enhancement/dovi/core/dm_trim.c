/* Display-management trim processing. */
#include "dm_trim.h"
#include "control_runtime.h"
#include "control_abi_recovered.h"
#include "byte_utils.h"
#include "fixed_access.h"

void dovi_ClampLevel2(int *level2)
{
    int a = level2[1], b = level2[2], c = level2[3], d = level2[4], e = level2[5], f;
    if (a > 2047)
        a = 2047;
    if (a < -2048)
        a = -2048;
    level2[1] = a;
    if (b > 2047)
        b = 2047;
    if (b < -2048)
        b = -2048;
    level2[2] = b;
    if (c > 2047)
        c = 2047;
    if (c < -2048)
        c = -2048;
    level2[3] = c;
    if (d > 65535)
        d = 65535;
    if (d < 0)
        d = 0;
    level2[4] = d;
    if (e < 2048)
        e = 2048;
    else if (e > 6143)
        e = 6143;
    level2[5] = e;
    f = level2[6];
    if (f > 8191)
        f = 8191;
    if (f < 0)
        f = 0;
    level2[6] = f;
}

int dovi_dm_CommitTrim(unsigned short target_max_pq, void *extensions, void *dm, void *tone_control)
{
    unsigned char *trim = (unsigned char *)extensions + 88u;
    unsigned short *values = (unsigned short *)(void *)fixed_load_le64((const unsigned char *)trim +
                                                                       (8u)),
                   indices[17], keys[17];
    int selected[7], dim = (short)fixed_load_le16((const unsigned char *)trim + (2u)),
                     count = (short)fixed_load_le16((const unsigned char *)trim + (6u)), i, j, k, n,
                     lower = 0, upper = 0;
    unsigned int bubble = (unsigned short)count;
    int swapped;
    unsigned char *tm = (unsigned char *)dm + DM_CONTEXT_DMCFG_OFFSET + DM_CONFIG_TMCTRL_OFFSET;
    int *level = (int *)((unsigned char *)tone_control + 116u);
    values[0] = (unsigned short)fixed_load_le16((const unsigned char *)extensions + (74u));
    for (i = 0; i <= count; ++i) {
        indices[i] = (unsigned short)i;
        keys[i] = values[i * dim];
    }
    if (bubble != 0u)
        do {
            swapped = 0;
            for (i = 0; i < (int)bubble; ++i)
                if (keys[i + 1] <= keys[i]) {
                    unsigned short tmp = keys[i];
                    keys[i] = keys[i + 1];
                    keys[i + 1] = tmp;
                    tmp = indices[i];
                    indices[i] = indices[i + 1];
                    indices[i + 1] = tmp;
                    swapped = 1;
                }
        } while (swapped && --bubble != 0u);
    if (keys[0] < target_max_pq && target_max_pq < keys[count]) {
        upper = 1;
        while (keys[upper] <= target_max_pq)
            ++upper;
        lower = upper - 1;
        if (upper > count) {
            dovi_control_services.trim.assertion(
                "/mnt/fileroot/yao.liu/androidq/hardware/amlogic/dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmCmmtFxp.c",
                "InterpolateTrim", 0x80a, "idx2 <= pTrim->TrimNum");
            dovi_control_services.trim.dump_stack();
        }
        if (keys[upper] != keys[lower]) {
            j = (int)indices[lower] * dim;
            k = (int)indices[upper] * dim;
            for (i = 1; i < dim; ++i) {
                int difference = (int)values[k + i] - (int)values[j + i];
                int numerator = (int)((unsigned int)difference * 32768u),
                    denominator = (int)values[k] - (int)values[j];
                int slope = denominator ? numerator / denominator : numerator;
                selected[i] =
                    (int)values[j + i] +
                    ((int)((unsigned int)slope * ((unsigned int)target_max_pq - values[j])) >> 15);
            }
        } else {
            n = indices[lower] * dim;
            for (i = 1; i < dim; ++i)
                selected[i] = values[n + i];
        }
    } else {
        n = indices[keys[0] >= target_max_pq ? 0 : count] * dim;
        for (i = 1; i < dim; ++i)
            selected[i] = values[n + i];
    }
    for (i = 1; i < dim - 1; ++i) {
        /* The function starts at ValueAdj2[6] and Default2[6]. */
        level[i] = (int)((unsigned int)selected[i] +
                         fixed_load_le32((const unsigned char *)tm + (96u + (unsigned int)i * 4u)) +
                         fixed_load_le32((const unsigned char *)tm + (68u + (unsigned int)i * 4u)));
    }
    if (fixed_load_le32((const unsigned char *)tone_control + (56u)) == 1u &&
        fixed_load_le32((const unsigned char *)tone_control + (60u)) == 0u) {
        level[5] =
            (int)((unsigned int)level[5] - fixed_load_le32((const unsigned char *)tm + (88u)));
        level[5] =
            (int)((unsigned int)level[5] + fixed_load_le32((const unsigned char *)tm + (124u)));
    }
    level[4] =
        (int)(((unsigned int)selected[4] + fixed_load_le32((const unsigned char *)tm + (112u)) +
               fixed_load_le32((const unsigned char *)tm + (84u))) *
              16u);
    level[6] =
        (int)((unsigned int)selected[6] * 2u + fixed_load_le32((const unsigned char *)tm + (120u)) +
              fixed_load_le32((const unsigned char *)tm + (92u)));
    dovi_ClampLevel2(level);
    level[6] -= 4096;
    return 0;
}
