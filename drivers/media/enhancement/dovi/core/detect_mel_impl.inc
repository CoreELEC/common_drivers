/* Identify minimal enhancement layer payloads from their NLQ parameters. */
#include "nlq_layout.h"
typedef char require_32bit_uint[(sizeof(unsigned int) == 4) ? 1 : -1];

static unsigned int read_le16(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static unsigned int read_le32(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
}

MEL_PAYLOAD_SCOPE int dovi_detect_mel_payload(const unsigned char *payload)
{
    unsigned int component;

    for (component = 0; component < 3; ++component) {
        const unsigned char *entry =
            payload + DOVI_NLQ_PARAMETERS_OFFSET + component * DOVI_NLQ_PARAMETER_BYTES;

        /*
         * Bytes +2/+3 are ignored when reading the 16-bit field.
         * Read each field at its encoded width in comparison order.
         */
        if (read_le16(entry + DOVI_NLQ_OFFSET) != 0 ||
            read_le32(entry + DOVI_NLQ_LINEAR_SLOPE_INT) != 0 ||
            read_le32(entry + DOVI_NLQ_LINEAR_SLOPE_FRACTION) != 0 ||
            read_le32(entry + DOVI_NLQ_VDR_IN_MAX_INT) != 1 ||
            read_le32(entry + DOVI_NLQ_VDR_IN_MAX_FRACTION) != 0 ||
            read_le32(entry + DOVI_NLQ_LINEAR_THRESHOLD_INT) != 0 ||
            read_le32(entry + DOVI_NLQ_LINEAR_THRESHOLD_FRACTION) != 0)
            return 0;
    }
    return 1;
}
