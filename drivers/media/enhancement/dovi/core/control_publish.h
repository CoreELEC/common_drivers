#ifndef DOVI_CONTROL_PUBLISH_H
#define DOVI_CONTROL_PUBLISH_H
#include "control_setting_layout.h"

/* Core 1 register pairs and the HDR payload include unaligned eight-byte
 * fields in the host record. Publication retains their byte offsets and order. */
void dovi_publish_registers(int *setting_words, _Bool multi_core1, unsigned int primary_core,
                            unsigned char *graphics_lut);
void dovi_publish_dolby_metadata(int *setting_words);

void dovi_publish_hdr_metadata(int *setting_words, int *target_parameters,
                               union dovi_hdr_infoframe *infoframe);

void dovi_notify_content_info(void *context, const void *sources, unsigned long primary,
                              const int *source_flags, const void *metadata_slots,
                              const void *header_slots);

#endif
