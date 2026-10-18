/* Native module metadata event callback. */
#ifndef DOVI_METADATA_CALLBACKS_H
#define DOVI_METADATA_CALLBACKS_H
#include "parser_context.h"
/* Composer and metadata buffers provide 8192/1024 writable bytes. All copied
 * ranges are disjoint, callbacks are serialized, and timestamp/input lengths
 * are ignored. Frame count advances for every event, including errors. Output
 * thresholds retain native signedness and composer writes precede DM errors.
 * Counters live beside the singleton and share its lifetime.
 * Native copy-length widening is retained even for nonpositive lengths
 * outside normal serializer output.
 */
void dovi_md_parser_evt_handler_cb(void *context, dv_md_parser_evt_t event,
                                   const void *timestamp, const void *header,
                                   const void *payload, int payload_size,
                                   const void *dm, int dm_size, int output_result);
#endif
