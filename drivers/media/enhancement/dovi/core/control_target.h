#ifndef DOVI_CONTROL_TARGET_H
#define DOVI_CONTROL_TARGET_H

#include "control_abi_recovered.h"

/* VSVDB target conversion. target_param points to 88 writable bytes
 * (22 little-endian u32 words). Input is null or a readable EDID block:
 * at least 21 bytes for v0, 15 for long v1, 12 for short v1/v2. The function
 * has no physical length argument and ignores the advertised length on v0/v2;
 * callers should supply their full 32-byte VSVDB storage. Input and output
 * storage must not overlap. No null-target check
 * is made after valid EDID identification. Unsupported headers retain all
 * target bytes; unsupported versions retain the first 3 capability writes;
 * unsupported v1 lengths additionally retain the 4 luminance writes. */
int vsvdb_to_target_param(const unsigned char *vsvdb, void *target_param);

#endif
