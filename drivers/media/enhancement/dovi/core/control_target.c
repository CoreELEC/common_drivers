/* Display-block decoding and output-target configuration. */
#include "control_target.h"
#include "pq_conversion.h"
#include "fixed_access.h"

static void put_word(void *target, unsigned int offset, unsigned int value)
{
    fixed_store_le32((unsigned char *)target + offset, value);
}

#define PUT(field, value) put_word(target_param, CONTROL_TARGET_##field##_OFFSET, (value))

int vsvdb_to_target_param(const unsigned char *vsvdb, void *target_param)
{
    unsigned int length, version, tmin, tmax, min_pq, max_pq;
    if (vsvdb == (const unsigned char *)0 || vsvdb[1] != 1u || vsvdb[2] != 0x46u ||
        vsvdb[3] != 0xd0u || vsvdb[4] != 0u || (vsvdb[0] >> 5) != 7u)
        return -1;

    length = (vsvdb[0] & 31u) + 1u;
    version = vsvdb[5] >> 5;
    PUT(VSVDB_LENGTH, length);
    PUT(VSVDB_VERSION, version);
    PUT(SINK_DM_VERSION, (vsvdb[5] >> 2) & 7u);

    if (version == 0u) {
        min_pq = ((unsigned int)vsvdb[19] << 4) | (vsvdb[18] >> 4);
        max_pq = ((unsigned int)vsvdb[20] << 4) | (vsvdb[18] & 15u);
        PUT(TMINPQ, min_pq);
        PUT(TMAXPQ, max_pq);
        PUT(TMIN,
            (unsigned int)((unsigned long long)dovi_PQ12ToL((unsigned short)min_pq) * 10000u >>
                           18));
        PUT(TMAX,
            (unsigned int)((unsigned long long)dovi_PQ12ToL((unsigned short)max_pq) * 10000u >>
                           18));
        PUT(RX, (((unsigned int)vsvdb[7] << 4) | (vsvdb[6] >> 4)) << 14);
        PUT(RY, (((unsigned int)vsvdb[8] << 4) | (vsvdb[6] & 15u)) << 14);
        PUT(GX, (((unsigned int)vsvdb[10] << 4) | (vsvdb[9] >> 4)) << 14);
        PUT(GY, (((unsigned int)vsvdb[11] << 4) | (vsvdb[9] & 15u)) << 14);
        PUT(BX, (((unsigned int)vsvdb[13] << 4) | (vsvdb[12] >> 4)) << 14);
        PUT(BY, (((unsigned int)vsvdb[14] << 4) | (vsvdb[12] & 15u)) << 14);
        PUT(WX, (((unsigned int)vsvdb[16] << 4) | (vsvdb[15] >> 4)) << 14);
        PUT(WY, (((unsigned int)vsvdb[17] << 4) | (vsvdb[15] & 15u)) << 14);
        return 0;
    }

    if (version == 1u) {
        tmin = (vsvdb[7] >> 1) * (vsvdb[7] >> 1) * 10000u / 16129u;
        tmax = (vsvdb[6] >> 1) * 500000u + 1000000u;
        PUT(TMIN, tmin);
        PUT(TMAX, tmax);
        PUT(TMINPQ, dovi_LToPQ12((unsigned int)(((unsigned long long)tmin << 18) / 10000u)));
        PUT(TMAXPQ, dovi_LToPQ12((unsigned int)(((unsigned long long)tmax << 18) / 10000u)));
        if (length == 12u) {
            PUT(V1_LOW_LATENCY, vsvdb[8] & 3u);
            PUT(RX, ((vsvdb[11] >> 3) | 0xa0u) << 18);
            PUT(RY, (((vsvdb[10] & 1u) << 1) | ((vsvdb[11] & 7u) << 2) |
                     (vsvdb[9] & 1u) | 0x40u)
                        << 18);
            PUT(GX, (unsigned int)(vsvdb[9] >> 1) << 18);
            PUT(GY, ((vsvdb[10] >> 1) | 0x80u) << 18);
            PUT(BX, ((vsvdb[8] >> 5) | 0x20u) << 18);
            PUT(BY, (((vsvdb[8] >> 2) & 7u) << 18) | 0x200000u);
        } else if (length == 15u) {
            PUT(RX, (unsigned int)vsvdb[9] << 18);
            PUT(RY, (unsigned int)vsvdb[10] << 18);
            PUT(GX, (unsigned int)vsvdb[11] << 18);
            PUT(GY, (unsigned int)vsvdb[12] << 18);
            PUT(BX, (unsigned int)vsvdb[13] << 18);
            PUT(BY, (unsigned int)vsvdb[14] << 18);
        } else {
            return -1;
        }
    } else if (version == 2u) {
        PUT(V2_INTERFACE, vsvdb[7] & 3u);
        PUT(V2_SUPPORTS_10B_12B_444, ((vsvdb[8] & 1u) << 1) | (vsvdb[9] & 1u));
        min_pq = (vsvdb[6] >> 3) * 20u;
        max_pq = (vsvdb[7] >> 3) * 65u + 2055u;
        PUT(TMINPQ, min_pq);
        PUT(TMAXPQ, max_pq);
        PUT(TMIN,
            (unsigned int)((unsigned long long)dovi_PQ12ToL((unsigned short)min_pq) * 10000u >>
                           18));
        PUT(TMAX,
            (unsigned int)((unsigned long long)dovi_PQ12ToL((unsigned short)max_pq) * 10000u >>
                           18));
        PUT(RX, ((vsvdb[10] >> 3) | 0xa0u) << 18);
        PUT(RY, ((vsvdb[11] >> 3) | 0x40u) << 18);
        PUT(GX, (unsigned int)(vsvdb[8] >> 1) << 18);
        PUT(GY, ((vsvdb[9] >> 1) | 0x80u) << 18);
        PUT(BX, ((vsvdb[10] & 7u) << 18) | 0x800000u);
        PUT(BY, ((vsvdb[11] & 7u) << 18) | 0x200000u);
    } else {
        return -1;
    }
    PUT(WX, 0x140346eu);
    PUT(WY, 0x150e560u);
    return 0;
}

#undef PUT
