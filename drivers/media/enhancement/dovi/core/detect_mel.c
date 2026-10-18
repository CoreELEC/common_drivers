#include "detect_mel.h"

#define MEL_PAYLOAD_SCOPE static
#include "detect_mel_impl.inc"
#undef MEL_PAYLOAD_SCOPE

int dovi_detect_mel(const struct dovi_decoder_prefix *decoder)
{
    return dovi_detect_mel_payload(decoder->root->payload);
}
