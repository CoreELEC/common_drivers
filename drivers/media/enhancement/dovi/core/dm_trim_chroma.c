/* Chroma coordinates and distances for trim interpolation. */
#include "dm_trim.h"
#include "fixed_access.h"
#include "fixed_arithmetic.h"
#include "pq_conversion.h"
#include "byte_utils.h"

typedef char trim_pointer_is_64[(sizeof(void *) == 8) ? 1 : -1];
typedef char trim_int_is_32[(sizeof(int) == 4) ? 1 : -1];
unsigned char chroma_distance_state[116];

const int chroma_xyz_to_lms[3][3] = {
    {107438129, 189943091, -21691083},
    {-60746434, 312812022, 12267708},
    {0, 0, 246484133},
};
const short chroma_lms_to_ictcp[3][3] = {
    {2048, 2048, 0},
    {3305, -6806, 3501},
    {17933, -17390, -543},
};

static long long mul64(long long a, long long b)
{
    return fixed_signed64((unsigned long long)a * (unsigned long long)b);
}

static long long add64(long long a, long long b)
{
    return fixed_signed64((unsigned long long)a + (unsigned long long)b);
}

static long long div64(long long a, long long b)
{
    if (!b)
        return 0;
    if (a == (-0x7fffffffffffffffll - 1) && b == -1)
        return a;
    return a / b;
}

unsigned int dovi_dm_FindChromaDist(const void *primaries, unsigned int luminance, int target_pq)
{
    int p[8], rx, ry, by, wy, difference[4], determinant, i, j;
    long long numer, scale = 0, xyz[3] = {0, 0, 0}, lms[3], crossed[3], sum;
    int pq[3], ictcp[3];
    for (i = 0; i < 8; ++i)
        p[i] = fixed_signed32(fixed_load_le32((const unsigned char *)primaries + i * 4));
    rx = p[0];
    ry = p[1];
    by = p[5];
    wy = p[7];
    difference[0] = fixed_signed32((unsigned int)rx - (unsigned int)p[4]);
    difference[1] = fixed_signed32((unsigned int)p[3] - (unsigned int)by);
    difference[2] = fixed_signed32((unsigned int)ry - (unsigned int)by);
    difference[3] = fixed_signed32((unsigned int)p[2] - (unsigned int)p[4]);
    numer = add64((long long)difference[0] * difference[1],
                  -((long long)difference[2] * difference[3]));
    determinant = fixed_signed32((unsigned int)(numer >> 26));
    if (determinant) {
        numer = add64(
            (long long)difference[1] * fixed_signed32((unsigned int)p[6] - (unsigned int)p[4]),
            -((long long)difference[3] * fixed_signed32((unsigned int)wy - (unsigned int)by)));
        scale = div64(numer, determinant);
    }
    if (wy) {
        scale = div64(fixed_signed64((unsigned long long)scale << 26), wy);
        xyz[0] = fixed_signed32((unsigned int)(mul64(scale, rx) >> 26));
        xyz[1] = fixed_signed32((unsigned int)(mul64(scale, ry) >> 26));
        xyz[2] = fixed_signed32((
            unsigned int)(mul64(scale,
                                fixed_signed32(0x4000000u - (unsigned int)rx - (unsigned int)ry)) >>
                          26));
        for (i = 0; i < 3; ++i)
            xyz[i] = mul64(xyz[i], luminance) >> 26;
    }
    dovi_MemCpyByte(chroma_distance_state, chroma_xyz_to_lms, 36);
    for (i = 0; i < 3; ++i)
        for (j = 0; j < 3; ++j)
            fixed_store_le32(chroma_distance_state + 40 + (i * 3 + j) * 4,
                             i == j ? 0x3ae16u : 0x28f5u);
    dovi_MemCpyByte(chroma_distance_state + 80, chroma_lms_to_ictcp, 18);
    for (i = 0; i < 3; ++i) {
        sum = 0;
        for (j = 0; j < 3; ++j)
            sum = add64(sum, mul64(xyz[j], chroma_xyz_to_lms[i][j]));
        lms[i] = sum >> 28;
    }
    for (i = 0; i < 3; ++i) {
        sum = 0;
        for (j = 0; j < 3; ++j)
            sum = add64(sum, mul64(lms[j], i == j ? 0x3ae16 : 0x28f5));
        crossed[i] = sum >> 18;
        if (crossed[i] < 0)
            crossed[i] = 0;
        pq[i] = dovi_L2PqX(fixed_signed64((unsigned long long)crossed[i] << 6));
        fixed_store_le32(chroma_distance_state + 104 + i * 4, (unsigned int)pq[i]);
    }
    for (i = 0; i < 3; ++i) {
        sum = 0;
        for (j = 0; j < 3; ++j)
            sum = add64(sum, (long long)pq[j] * chroma_lms_to_ictcp[i][j]);
        ictcp[i] = fixed_signed32((unsigned int)(sum >> 12));
    }
    return fixed_abs32((unsigned int)target_pq - (unsigned int)ictcp[0]) +
           fixed_abs32((unsigned int)ictcp[1]) + fixed_abs32((unsigned int)ictcp[2]);
}
