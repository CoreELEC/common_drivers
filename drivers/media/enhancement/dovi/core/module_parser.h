/* Host metadata-parser ownership and callback wrappers. */
#ifndef DOVI_MODULE_PARSER_H
#define DOVI_MODULE_PARSER_H

typedef int (*dovi_module_log_fn)(const char *format, ...);
struct dovi_module_parser_context;

/* One static, initially zeroed pipeline shares the parser singleton.
 * These entry points are non-reentrant; callers must serialize init, reset,
 * process and release. The logger must be nonnull, stable and non-reentrant.
 * Init returns the app-context pointer and preserves counters. Init/reset flags
 * are ignored. Release does not destroy, free or reset either singleton.
 *
 * Process retains metadata_callbacks.h's unchecked buffer requirements:
 * composer has 8192 writable bytes and metadata has 1024; input, outputs and
 * length slots are disjoint from the pipeline and each other. Supply readable
 * input/refill padding and a nonzero payload byte. Payload-only decoder ingress
 * uses one 4096-byte slot. A successful call publishes lengths and resets the
 * parser; failure returns -1 and leaves the caller's lengths unchanged.
 * No allocation, locking, ownership or source-status policy is added here.
 */
struct dovi_module_parser_context *dovi_metadata_parser_init(dovi_module_log_fn logger, int flag);
int dovi_metadata_parser_reset(int flag);
int dovi_metadata_parser_process(const char *input, int size, char *composer_output,
                                 int *composer_size, char *metadata_output, int *metadata_size,
                                 unsigned char eos);
void dovi_metadata_parser_release(void);

#endif
