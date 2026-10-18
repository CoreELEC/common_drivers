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
#include "control_memory.h"
#include "numeric_common.h"
#include "shared_data.h"
#include "kernel_map_composition.h"
#include "tone_mapping_math.h"

#define memset dovi_control_os.memset
#define _printk dovi_control_os.printk
#define dump_stack dovi_control_os.dump_stack
/* Named immutable data shared by the display-management stages. */
#define dovi_dm_assertion_format (dovi_retained_section_3.diagnostics.assertion_format)
#define dovi_dm_assertion_false (dovi_retained_section_3.diagnostics.assertion_false)
#define dovi_v8_primaries (dovi_retained_section_3.version8_primaries)
#define dovi_level2_luma_trim (dovi_retained_section_3.level2_luma_trim)
#define dovi_level8_luma_trim (dovi_retained_section_3.level8_luma_trim)
#define dovi_level2_chroma_trim (dovi_retained_section_3.level2_chroma_trim)
#define dovi_level8_chroma_trim (dovi_retained_section_3.level8_chroma_trim)
#define dovi_pulsar_gamma_to_linear (dovi_retained_section_3.pulsar_gamma_to_linear)
#define dovi_sdr_gamma_to_linear (dovi_retained_section_3.sdr_gamma_to_linear)
#define dovi_log10_value (dovi_retained_section_3.log10_value)
#define dovi_log10_rate (dovi_retained_section_3.log10_rate)
#define dovi_power10_value (dovi_retained_section_3.power10_value)
#define dovi_power10_scale (dovi_retained_section_3.power10_scale)
#endif
