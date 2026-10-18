#ifndef DOVI_RPU_VALIDATION_H
#define DOVI_RPU_VALIDATION_H

typedef int (*dovi_validation_log_fn)(const char *format, ...);

/*
 * The little-endian root object has header and payload pointers at +0 and +8.
 * Keep referenced storage allocated and accessible throughout the call.
 * Optional diagnostics use unsigned log levels >=3. Callback sequencing and
 * reloads follow each native validator; callbacks must satisfy its contract.
 */
/* STB 2.4 returns -4/-5 for unsupported level/profile indices. STB 2.6
 * retains fatal level/profile/pivot and whole-row search bounds, including
 * its separate profile-16 row. Spatial modes must be 0..7 in STB 2.4;
 * STB 2.6 traps for invalid modes. Diagnostics reload header bytes. */
int dovi_validate_level(const void *rpu_data, dovi_validation_log_fn log, unsigned int log_level);
int dovi_validate_spatial_filter(const void *rpu_data, dovi_validation_log_fn log,
                                 unsigned int log_level);
int dovi_validate_profile(const void *rpu_data, dovi_validation_log_fn log, unsigned int log_level);

#endif
