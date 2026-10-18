#ifndef DOVI_COMPOSER_VALIDATION_H
#define DOVI_COMPOSER_VALIDATION_H

typedef int (*dovi_composer_log_fn)(const char *format, ...);

/*
 * Read-only validators of stable little-endian composer/DM records. Callbacks
 * may record diagnostics but must not mutate input. Return 0 or -2 and continue
 * checking after errors. Composer storage spans 0x708 bytes; DM storage includes
 * the 0x4c-byte prefix and count (+0x4a) records of stride 0x108, even when count
 * 255 is diagnosed. Diagnostic text, order, widths and signedness are retained.
 */
int dovi_validate_dm_payload(const void *dm, unsigned int frame, dovi_composer_log_fn log,
                             unsigned int log_level);
int dovi_validate_composer_config(const void *composer, unsigned int frame,
                                  dovi_composer_log_fn log, unsigned int log_level);

#endif
