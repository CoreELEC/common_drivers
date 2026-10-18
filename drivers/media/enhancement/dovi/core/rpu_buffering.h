#ifndef DOVI_RPU_BUFFERING_H
#define DOVI_RPU_BUFFERING_H

#include "decoder_lifecycle.h"

enum rbsp_ingress_state {
    RBSP_SEARCH_RPU = 0,
    RBSP_FOUND_START_00 = 1,
    RBSP_FOUND_START_0000 = 2,
    RBSP_FOUND_START_000001 = 3,
    RBSP_FOUND_RPU = 4,
    RBSP_FOUND_00 = 5,
    RBSP_FOUND_0000 = 6
};

/*
 * Original decoder layout: byte storage pointer +0x10, uint32 length array
 * pointer +0x18, write/read indices +0x20/+0x24, flush word +0x28, direct
 * mode word +0x30, slot count +0x38, log callback/level +0x315c8/+0x315d0,
 * and framing state word +0x315d4. Slots have 0x1000-byte address spacing.
 * Valid indices, sufficient disjoint storage and serialized calls required.
 *
 * push returns 0, -7 for invalid arguments, -22 for a full queue, or -1 for
 * the 00 00 02 sequence inside framed payload. Buffered writes increment
 * the slot length before trapping if the previous length exceeded 0xfff
 * in STB 2.6. STB 2.4 writes first, then returns -22 if the resulting
 * signed slot length exceeds 0x1000.
 * Direct mode has no 0x1000-byte capacity check and scans trailing zeros
 * backwards without a start bound; caller-provided readable prefix/storage
 * must cover every accessed byte.
 */
int dovi_rpu_decoder_push_data(void *decoder, const unsigned char *data, int length,
                               int flush);
void dovi_dump_rpu_buffer_info(void *decoder);
int dovi_rpu_decoder_last_rpu_output(const void *decoder);

#endif
