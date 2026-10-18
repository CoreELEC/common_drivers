/* Packed control-path metadata and HDMI preparation operations. */
#ifndef DOVI_CONTROL_METADATA_H
#define DOVI_CONTROL_METADATA_H

#include "control_abi_recovered.h"

enum { CONTROL_METADATA_BASE_BYTES = 71, CONTROL_METADATA_EXT_BYTES = 261 };

/* The control representation is big-endian packed bytes. It differs from the
 * parser's 76-byte base / 264-byte extension representation. All objects must
 * cover the unchecked reads and writes. Offsets use 32-bit wrapping
 * arithmetic; an empty input does not force chroma or inspect old extensions.
 */
typedef int (*control_metadata_logger)(const char *format, unsigned int length);
int read_md_with_logger(void *metadata, const char *input, int size, int *offset,
                        control_metadata_logger logger);
int read_md(void *metadata, const char *input, int size, int *offset);
void filter_md(void *current, const void *previous);

/* The selected DM configuration must be valid. Getter failures are ignored,
 * retaining the original use of untouched local output. Trim reduction receives
 * the context's target pointer field and may change metadata and target state. */
void update_dm_l0_for_hdmi(void *context, void *metadata);
void update_dm_ext_for_hdmi(void *context, void *metadata);
void update_md_for_hdmi(void *context, void *metadata);

#endif
