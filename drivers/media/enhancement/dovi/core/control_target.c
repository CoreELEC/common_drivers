/* Display-block decoding and output-target configuration. */
#include "control_orchestration_bindings.h"
#include "fixed_access.h"
#include "control_target.h"

int dovi_decode_display_block(void *context, const unsigned char *vsvdb)
{
    long *control = context;
    unsigned int version;
    unsigned char packed_flags;
    unsigned int value;
    unsigned int block_length;
    if (vsvdb != (const unsigned char *)0x0) {
        value = 0xfffffff8;
        if ((((vsvdb[4] != '\0') || (vsvdb[3] != 0xd0)) || (vsvdb[2] != 'F')) ||
            ((vsvdb[1] != '\x01' || ((*vsvdb & 0xe0) != 0xe0))))
            return (int)value;
        block_length = *vsvdb & 0x1f;
        *(unsigned int *)(control + 0x28f) = block_length + 1;
        packed_flags = vsvdb[5];
        version = (unsigned int)(packed_flags >> 5);
        *(unsigned int *)((long)control + 0x147c) = version;
        *(unsigned int *)((long)control + 0x1484) = vsvdb[5] >> 2 & 7;
        *(unsigned int *)(control + 0x290) = vsvdb[5] & 1;
        if (version == 2) {
            *(unsigned int *)((long)control + 0x148c) = vsvdb[7] & 3;
            *(unsigned int *)(control + 0x292) = (vsvdb[9] & 1) | (vsvdb[8] & 1) << 1;
            value = (unsigned int)(vsvdb[6] >> 3) * 0x14;
            *(short *)((long)control + 0x149c) = (short)value;
            *(unsigned short *)((long)control + 0x149e) = (vsvdb[7] >> 3) * 65u + 2055u;
            value = dovi_PQ12ToL(value);
            *(unsigned int *)((long)control + 0x1494) = value;
            value = dovi_PQ12ToL((unsigned int)*(unsigned short *)((long)control + 0x149e));
            *(unsigned int *)(control + 0x293) = value;
            *(unsigned int *)(control + 0x294) =
                ((vsvdb[10] >> 3) | 0xa0u) << 18;
            *(unsigned int *)((long)control + 0x14a4) = ((vsvdb[11] >> 3) | 0x40u) << 18;
            *(unsigned int *)(control + 0x295) = (unsigned int)(vsvdb[8] >> 1) << 18;
            *(unsigned int *)((long)control + 0x14ac) = ((vsvdb[9] >> 1) | 0x80u) << 18;
            *(unsigned int *)(control + 0x296) = ((vsvdb[10] & 7u) << 18) | 0x800000u;
            packed_flags = vsvdb[0xb];
        finish_packed_primaries:
            value = 0x150e560;
            *(unsigned int *)((long)control + 0x14b4) = ((packed_flags & 7u) << 18) | 0x200000u;
            *(unsigned int *)(control + 0x297) = 0x140346e;
        } else if (version == 1) {
            value = (int)((unsigned int)(vsvdb[7] >> 1) * (unsigned int)(vsvdb[7] >> 1) * 0x40000) /
                    0x3f01;
            *(unsigned int *)((long)control + 0x1494) = value;
            *(unsigned int *)(control + 0x293) =
                (unsigned int)(vsvdb[6] >> 1) * 0xc80000 + 0x1900000;
            value = dovi_LToPQ12(value);
            *(short *)((long)control + 0x149c) = (short)value;
            value = dovi_LToPQ12(*(unsigned int *)(control + 0x293));
            *(short *)((long)control + 0x149e) = (short)value;
            if (block_length != 0xe) {
                if (block_length != 0xb) {
                    value = 0xfffffff8;
                    return (int)value;
                }
                *(unsigned int *)(control + 0x291) = vsvdb[8] & 3;
                *(unsigned int *)(control + 0x294) =
                    ((vsvdb[11] >> 3) | 0xa0u) << 18;
                *(unsigned int *)((long)control + 0x14a4) =
                    (((vsvdb[10] & 1u) << 1) | ((vsvdb[11] & 7u) << 2) |
                     (vsvdb[9] & 1u) | 0x40u) << 18;
                *(unsigned int *)(control + 0x295) = (unsigned int)(vsvdb[9] >> 1) << 18;
                *(unsigned int *)((long)control + 0x14ac) = ((vsvdb[10] >> 1) | 0x80u) << 18;
                *(unsigned int *)(control + 0x296) = ((vsvdb[8] >> 5) | 0x20u) << 18;
                packed_flags = vsvdb[8] >> 2;
                goto finish_packed_primaries;
            }
            value = 0x150e560;
            *(unsigned int *)(control + 0x294) = (unsigned int)vsvdb[9] << 18;
            *(unsigned int *)((long)control + 0x14a4) = (unsigned int)vsvdb[10] << 18;
            *(unsigned int *)(control + 0x295) = (unsigned int)vsvdb[0xb] << 18;
            *(unsigned int *)((long)control + 0x14ac) = (unsigned int)vsvdb[0xc] << 18;
            *(unsigned int *)(control + 0x296) = (unsigned int)vsvdb[0xd] << 18;
            packed_flags = vsvdb[0xe];
            *(unsigned int *)(control + 0x297) = 0x140346e;
            *(unsigned int *)((long)control + 0x14b4) = (unsigned int)packed_flags << 18;
        } else {
            if (packed_flags >> 5 != 0)
                return (int)value;
            value = (unsigned int)(vsvdb[0x12] >> 4) | (unsigned int)vsvdb[0x13] << 4;
            *(short *)((long)control + 0x149c) = (short)value;
            *(unsigned short *)((long)control + 0x149e) =
                (vsvdb[0x12] & 0xf) | (unsigned short)vsvdb[0x14] << 4;
            value = dovi_PQ12ToL(value);
            *(unsigned int *)((long)control + 0x1494) = value;
            value = dovi_PQ12ToL((unsigned int)*(unsigned short *)((long)control + 0x149e));
            *(unsigned int *)(control + 0x293) = value;
            *(unsigned int *)(control + 0x294) = (((unsigned int)vsvdb[7] << 4) | (vsvdb[6] >> 4)) << 14;
            *(unsigned int *)((long)control + 0x14a4) =
                (((unsigned int)vsvdb[8] << 4) | (vsvdb[6] & 15u)) << 14;
            *(unsigned int *)(control + 0x295) = (((unsigned int)vsvdb[10] << 4) | (vsvdb[9] >> 4)) << 14;
            *(unsigned int *)((long)control + 0x14ac) =
                (((unsigned int)vsvdb[11] << 4) | (vsvdb[9] & 15u)) << 14;
            *(unsigned int *)(control + 0x296) =
                (((unsigned int)vsvdb[13] << 4) | (vsvdb[12] >> 4)) << 14;
            *(unsigned int *)((long)control + 0x14b4) =
                (((unsigned int)vsvdb[14] << 4) | (vsvdb[12] & 15u)) << 14;
            *(unsigned int *)(control + 0x297) =
                (((unsigned int)vsvdb[16] << 4) | (vsvdb[15] >> 4)) << 14;
            value = (((unsigned int)vsvdb[17] << 4) | (vsvdb[15] & 15u)) << 14;
        }
        *(unsigned int *)((long)control + 0x14bc) = value;
    }
    return 0;
}

void dovi_configure_output_target(void *context, const unsigned char *input_configuration,
                                  const int *target_parameters, const unsigned char *vsvdb)
{
    long *control = context;
    unsigned int chroma_format;
    _Bool low_latency;
    unsigned short maximum_pq;
    unsigned int value;
    int format;
    unsigned int output_mode;
    unsigned int chroma_mode;
    output_mode = 0;
    *(int *)((long)control + 0xb4) = target_parameters[1];
    format = *target_parameters;
    *(unsigned int *)(control + 0x1a) = 0;
    *(int *)(control + 0x17) = format;
    format = (unsigned int)(target_parameters[9] != 0) << 1;
    if (target_parameters[9] == 1) {
        format = 1;
    }
    *(int *)(control + 0x19) = format;
    format = target_parameters[9];
    if (format != 0) {
        if (format == 1) {
            output_mode = 2;
        } else if (format != 2) {
            output_mode = 1;
        }
    }
    *(unsigned int *)((long)control + 0xcc) = output_mode;
    *(unsigned int *)((long)control + 0xc4) = 0;
    *(unsigned int *)((long)control + 0xd4) = 0xc;
    *(unsigned int *)(control + 0xc) = (unsigned int)(*(const int *)(input_configuration + 4) == 0);
    *(unsigned int *)((long)control + 100) = *(const unsigned int *)(input_configuration + 8);
    if (vsvdb == (const unsigned char *)0x0) {
    configure_standard_output:
        *(unsigned int *)((long)control + 0x143c) = 0;
        *(unsigned int *)(control + 0xd) = 0;
        *(unsigned int *)((long)control + 0xc4) = (unsigned int)(target_parameters[10] != 0);
        format = (unsigned int)(target_parameters[9] != 0) << 1;
        if (target_parameters[9] == 1) {
            format = 1;
        }
        *(int *)(control + 0x19) = format;
        value = 2;
        if (target_parameters[9] != 1) {
            value = (unsigned int)(target_parameters[9] != 0);
        }
        *(unsigned int *)((long)control + 0xe4) = 1;
        *(unsigned int *)((long)control + 0xcc) = value;
        *(int *)(control + 0x1d) = target_parameters[0x15];
        *(int *)((long)control + 0xec) = target_parameters[0x16];
        *(int *)(control + 0x1e) = target_parameters[0x17];
        *(int *)((long)control + 0xf4) = target_parameters[0x18];
        *(int *)(control + 0x1f) = target_parameters[0x19];
        *(int *)((long)control + 0xfc) = target_parameters[0x1a];
        *(int *)(control + 0x20) = target_parameters[0x1b];
        *(int *)((long)control + 0x104) = target_parameters[0x1c];
        *(int *)(control + 0x2e) = (int)(short)target_parameters[0x14];
        *(short *)((long)control + 0x15c) = (short)target_parameters[0xc];
        *(unsigned short *)((long)control + 0x15e) =
            *(const unsigned short *)((long)target_parameters + 0x32);
        *(short *)(control + 0x2c) = (short)target_parameters[0xd];
        *(unsigned short *)((long)control + 0x162) =
            *(const unsigned short *)((long)target_parameters + 0x36);
        *(short *)((long)control + 0x164) = (short)target_parameters[0xe];
        *(unsigned short *)((long)control + 0x166) =
            *(const unsigned short *)((long)target_parameters + 0x3a);
        *(short *)(control + 0x2d) = (short)target_parameters[0xf];
        *(unsigned short *)((long)control + 0x16a) =
            *(const unsigned short *)((long)target_parameters + 0x3e);
        format = target_parameters[0x10];
        *(unsigned int *)(control + 0x2b) = 1;
        *(unsigned int *)((long)control + 0x174) = 1;
        *(unsigned int *)(control + 0x32) = 0;
        *(short *)((long)control + 0x16c) = (short)format;
        *(int *)(control + 0x2f) = target_parameters[0x11] << 3;
        *(int *)((long)control + 0x17c) = target_parameters[0x12] << 3;
        *(int *)(control + 0x30) = target_parameters[0x13] << 3;
        output_mode = 3;
        if (target_parameters[6] != 2) {
            output_mode = 0;
        }
        if (target_parameters[6] == 1) {
            output_mode = 1;
        }
        *(unsigned int *)(control + 0x34) = output_mode;
        *(unsigned int *)(control + 0x2a) = (unsigned int)(target_parameters[0xb] == 1);
        *(int *)((long)control + 0x1a4) = target_parameters[7];
        value = target_parameters[4];
        *(unsigned int *)(control + 0x35) = value;
        if (target_parameters[6] == 0) {
            format = 0x1900000;
        } else {
            format = target_parameters[5];
        }
        *(int *)((long)control + 0x1ac) = format;
        value = dovi_LToPQ12(value);
        *(short *)(control + 0x36) = (short)value;
        value = dovi_LToPQ12(*(unsigned int *)((long)control + 0x1ac));
        *(short *)((long)control + 0x1b2) = (short)value;
        if (target_parameters[6] == 0) {
            output_mode = 3;
            if (target_parameters[8] != 10) {
                output_mode = 4;
            }
        } else {
            output_mode = 2;
        }
        *(unsigned int *)(control + 0x28a) = output_mode;
        control[0x19] = 2;
    } else {
        format = *(int *)((long)control + 0x147c);
        if (format == 0) {
        disable_low_latency:
            output_mode = 0;
            low_latency = 0;
        } else {
            if (format != 2) {
                if ((format == 1) &&
                    (((int)control[0x28f] == 0xf || ((int)control[0x28f] == 0xc)))) {
                    if ((int)control[0x291] == 0)
                        goto disable_low_latency;
                    if ((int)control[0x291] == 1)
                        goto select_low_latency;
                }
                *(unsigned int *)(control + 0x399) = 0;
                goto configure_standard_output;
            }
            output_mode = *(unsigned int *)((long)control + 0x148c);
            low_latency = 1;
            switch (output_mode) {
            case 0:
                break;
            case 1:
                output_mode = 0;
                if (target_parameters[3] != 0) {
                    if ((0x780 < *target_parameters) || (0x438 < target_parameters[1]))
                        goto use_default_rgb_mode;
                    if ((int)control[0x292] == 1) {
                    configure_rgb_10bit:
                        low_latency = 1;
                        *(unsigned int *)((long)control + 0xc4) = 1;
                        output_mode = 1;
                        fixed_store_le64((unsigned char *)control + 0x1454, 0x100000001);
                        *(unsigned int *)((long)control + 0xd4) = 10;
                        control[0x19] = 0x100000002;
                    } else {
                        if ((int)control[0x292] != 2)
                            goto use_default_rgb_mode;
                    configure_rgb_12bit:
                        output_mode = 1;
                        fixed_store_le64((unsigned char *)control + 0x1454, 0x100000001);
                        low_latency = 1;
                        *(unsigned int *)((long)control + 0xc4) = 1;
                        *(unsigned int *)((long)control + 0xd4) = 0xc;
                        control[0x19] = 0x100000002;
                    }
                }
                break;
            case 2:
            select_low_latency:
                output_mode = 0;
                low_latency = target_parameters[2] != 0;
                break;
            case 3:
                low_latency = target_parameters[2] != 0;
                if (target_parameters[3] != 0) {
                    if ((0x780 < *target_parameters) || (0x438 < target_parameters[1])) {
                        output_mode = 0;
                        low_latency = 1;
                        break;
                    }
                    low_latency = 1;
                    if ((int)control[0x292] == 1)
                        goto configure_rgb_10bit;
                    if ((int)control[0x292] == 2)
                        goto configure_rgb_12bit;
                }
            use_default_rgb_mode:
                output_mode = 0;
                break;
            default:
                output_mode = 0;
                low_latency = 0;
            }
        }
        *(unsigned int *)(control + 0x399) = 0;
        chroma_mode = 3;
        if (*(int *)((long)control + 0x1484) != 1) {
            chroma_mode = 4;
        }
        chroma_format = 2;
        if (*(int *)((long)control + 0x1484) != 0) {
            chroma_format = chroma_mode;
        }
        *(unsigned int *)((long)control + 0x8dc) = chroma_format;
        if (!low_latency) {
            *(unsigned int *)((long)control + 0xc4) = 3;
            maximum_pq = *(unsigned short *)((long)control + 0x149e);
            *(unsigned int *)(control + 0x1c) = 0;
            *(unsigned int *)(control + 0x28a) = 5;
            *(unsigned int *)(control + 0x34) = 1;
            control[0x2a] = 0x200000001;
            *(unsigned int *)(control + 0x35) = *(unsigned int *)((long)control + 0x1494);
            *(int *)((long)control + 0x1ac) = (int)control[0x293];
            *(unsigned short *)(control + 0x36) = *(unsigned short *)((long)control + 0x149c);
        } else {
            *(unsigned int *)(control + 0x28a) = 2;
            *(unsigned int *)(control + 0x2a) = output_mode;
            *(unsigned int *)((long)control + 0x145c) = 1;
            *(unsigned int *)((long)control + 0xe4) = 1;
            *(int *)(control + 0x1d) = (int)control[0x294];
            *(unsigned int *)((long)control + 0xec) = *(unsigned int *)((long)control + 0x14a4);
            control[0x21] = 0x1a;
            *(unsigned int *)(control + 0x399) = 1;
            *(int *)(control + 0x1f) = (int)control[0x296];
            *(unsigned int *)((long)control + 0xfc) = *(unsigned int *)((long)control + 0x14b4);
            control[0x1e] = control[0x295];
            *(unsigned int *)((long)control + 0x154) = 3;
            *(unsigned int *)((long)control + 0x104) = *(unsigned int *)((long)control + 0x14bc);
            *(int *)(control + 0x20) = (int)control[0x297];
            *(unsigned int *)(control + 0x34) = 1;
            *(unsigned int *)(control + 0x35) = *(unsigned int *)((long)control + 0x1494);
            *(int *)((long)control + 0x1ac) = (int)control[0x293];
            value = dovi_LToPQ12(*(unsigned int *)((long)control + 0x1494));
            *(short *)(control + 0x36) = (short)value;
            value = dovi_LToPQ12(*(unsigned int *)((long)control + 0x1ac));
            maximum_pq = (unsigned short)value;
            *(unsigned int *)(control + 0xc) = 0;
        }
        value = (unsigned int)low_latency;
        *(unsigned short *)((long)control + 0x1b2) = maximum_pq;
        *(unsigned int *)((long)control + 0x143c) = value;
        *(unsigned int *)(control + 0xd) = value;
    }
}
