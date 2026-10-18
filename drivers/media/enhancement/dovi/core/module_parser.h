/* Host metadata-parser ownership and callback wrappers. */
#ifndef DOVI_MODULE_PARSER_H
#define DOVI_MODULE_PARSER_H

typedef int (*dovi_module_log_fn)(const char *format, ...);

#define DOVI_MODULE_PARSER_CONTEXT_SIZE 0x770u
#define DOVI_MODULE_PARSER_COMPOSER_CAPACITY 0x2000u
#define DOVI_MODULE_PARSER_METADATA_CAPACITY 0x400u

/* The kernel bridge or host adapter defines this table before any call.
 * Allocation returns stable, writable, suitably aligned storage or NULL.
 * Free and printk retain their original ordering. printf-style arguments
 * contain integers and pointers only, and all supplied callbacks are valid.
 */
struct dovi_module_imports {
    void *(*vmalloc)(unsigned long long size);
    void (*vfree)(void *pointer);
    int (*printk)(const char *format, ...);
};
extern struct dovi_module_imports dovi_module_os;
extern unsigned int dovi_module_frame_nr;

void *dovi_metadata_parser_init(void);
void dovi_metadata_parser_release(void **context);
int dovi_metadata_parser_reset(void *context);
int dovi_metadata_parser_process(void *context, const unsigned char *input, unsigned int size,
                                 void *composer_output, unsigned int *composer_size,
                                 void *metadata_output, unsigned int *metadata_size,
                                 unsigned int flags, int input_kind);

#include "metadata_callbacks.h"

#endif
