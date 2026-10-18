/* Spatial filter validation. */
#ifndef DOVI_SPATIAL_VALIDATION_H
#define DOVI_SPATIAL_VALIDATION_H
#include "rpu_validation.h"
/* rpu_data+0 points to a readable 310-byte header. Modes index eight rows.
 * STB 2.6 retains fatal mode and whole-table bounds; STB 2.4 requires valid
 * indices. Diagnostics re-read header bytes after the first log call.
 */
#endif
