/* Preserve the host logger and log-level types around the shared validators. */
#include "composer_validation.h"

typedef dovi_composer_log_fn validation_log_fn;
typedef unsigned int validation_log_level;
#include "composer_validation_impl.inc"
