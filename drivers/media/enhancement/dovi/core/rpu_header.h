#ifndef DOVI_RPU_HEADER_H
#define DOVI_RPU_HEADER_H

#include "decoder_lifecycle.h"

#define DOVI_RPU_HEADER_SIZE 0x136u
#define DOVI_RPU_HEADER_DECODER_MIN_SIZE 0x316e0u

typedef int (*dovi_rpu_header_log_fn)(const char *format, ...);
typedef dec_event_callback_t dovi_rpu_header_event_fn;

/*
 * decoder+0 points to a root whose first pointer is the writable header;
 * decoder+0x40 holds struct dovi_bitstream. Sequence fields are cached at
 * +0x64..+0x74, presence at +0x78 and the ID bitmap at +0x7c. Logging callback
 * and level are at +0x315c8/+0x315d0; the event callback is at +0x316d8.
 * Callbacks do not retain messages. Supply stable, disjoint writable objects
 * and padded input as in bitstream.h.
 *
 * Returns a wrapping signed bit count or -3. Skipped fields retain contents,
 * byte fields wrap. With PARSER_FATAL_ARRAY_LIMITS, the pivot reader traps after
 * consuming a seventeenth pivot; otherwise pivot array indexing is unchecked.
 */
int dovi_rpu_header_read(void *decoder);

#endif
