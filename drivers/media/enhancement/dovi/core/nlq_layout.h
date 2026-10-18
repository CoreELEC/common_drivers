/* Nonlinear quantization records within each decoded RPU payload partition. */
#ifndef DOVI_NLQ_LAYOUT_H
#define DOVI_NLQ_LAYOUT_H

/* Prediction flags and distances are bytes indexed by pivot * 3 + component.
 * Parameters are 28-byte records; coefficient references are byte offsets
 * into the partition array. Fractional fields retain their raw 32-bit bits.
 * The linear-deadzone and mu-law parameters share the same 16-byte union. */
enum {
    DOVI_RPU_PAYLOAD_PARTITION_BYTES = 0x7038,
    DOVI_NLQ_PREDICTION_FLAGS_OFFSET = 0x6844,
    DOVI_NLQ_PARAMETERS_OFFSET = 0x6874,
    DOVI_NLQ_PARAMETER_BYTES = 0x1c,
    DOVI_NLQ_PREDICTION_DISTANCE_OFFSET = 0x6d60,
    DOVI_NLQ_COEFFICIENT_REFERENCES_OFFSET = 0x6f74,
    DOVI_NLQ_OFFSET = 0,
    DOVI_NLQ_VDR_IN_MAX_INT = 4,
    DOVI_NLQ_VDR_IN_MAX_FRACTION = 8,
    DOVI_NLQ_LINEAR_SLOPE_INT = 12,
    DOVI_NLQ_LINEAR_SLOPE_FRACTION = 16,
    DOVI_NLQ_LINEAR_THRESHOLD_INT = 20,
    DOVI_NLQ_LINEAR_THRESHOLD_FRACTION = 24,
    DOVI_NLQ_MU_LAW_PIVOT = 12,
    DOVI_NLQ_MU_LAW_MU = 16,
    DOVI_NLQ_UNION_OFFSET = DOVI_NLQ_PARAMETERS_OFFSET + 12,
    DOVI_NLQ_MU_LAW_MU_OFFSET = DOVI_NLQ_PARAMETERS_OFFSET + 16,
};

#endif
