#ifndef DOVI_RPU_VALIDATION_H
#define DOVI_RPU_VALIDATION_H

typedef int (*dovi_validation_log_fn)(const char *format, ...);

/*
 * The little-endian root object has header and payload pointers at +0 and +8.
 * Keep referenced storage allocated and accessible throughout the call.
 * Optional diagnostics use unsigned log levels >=3. Callback sequencing and
 * reloads follow each native validator; callbacks must satisfy its contract.
 */
/* Unsupported level/profile indices return -4/-5, respectively.
 * Spatial modes must be 0..7; callers provide valid readable table indices.
 * Optional diagnostics reload header bytes after the first log callback.
 * Keep root and payload storage readable throughout callback execution. */
int dovi_validate_level(const void *rpu_data, dovi_validation_log_fn log, unsigned int log_level);
int dovi_validate_spatial_filter(const void *rpu_data, dovi_validation_log_fn log,
                                 unsigned int log_level);
int dovi_validate_profile(const void *rpu_data, dovi_validation_log_fn log, unsigned int log_level);

#endif
