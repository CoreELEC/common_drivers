/* Canonical interfaces used by the display-management implementations. */
#ifndef DOVI_DM_BINDINGS_H
#define DOVI_DM_BINDINGS_H
#include "fixed_access.h"
#include "dm_pipeline.h"
#include "dm_configuration.h"
#include "dm_frame_format.h"
#include "dm_transfer.h"
#include "color_basis.h"
#include "matrix_arithmetic.h"
#include "color_narrow.h"
#include "color_primaries.h"
#include "numeric_color_matrix.h"
#include "fixed_arithmetic.h"
#include "fixed_transcendentals.h"
#include "pq_conversion.h"
#include "numeric_transfer.h"
#include "numeric_helpers.h"
#include "tone_curve.h"
#include "tone_controls.h"
#include "byte_utils.h"

static inline int dm_pair(unsigned short lo, unsigned short hi)
{
    return fixed_signed32((unsigned int)lo | ((unsigned int)hi << 16));
}

#endif
