#ifndef DOVI_CONTROL_PACKETS_H
#define DOVI_CONTROL_PACKETS_H

/* Original STB 2.6 byte layouts. All storage is ordinary readable/writable
 * memory; packet/state buffers must be disjoint unless a function says otherwise. */
unsigned int dovi_GetByteField(unsigned int value, unsigned int mask);
unsigned int dovi_get_data_set_length(const void *packet);
int dovi_vsem_unpack(const void *packets, void *payload);
int dovi_get_info_frame(const void *state, const void *target, const void *metadata,
                        void *infoframe);
int dovi_get_output_format(const void *state, void *hdmi_format, void *core_format);
int dovi_update_src_param(void *source, int format);
int dovi_set_default_src_param(void *source, int format);
int dovi_set_default_target_param(void *target, int format);
long long dovi_get_dolby_control_data(void *state, void *packet, void *is_standard, void *control);
void dovi_dmKs2dmreg_core3(const void *state, const void *kernel, void *registers,
                           const void *metadata, unsigned int length);
void dovi_parse_vsif_from_PB4(const void *packet, void *control);
unsigned int dovi_get_dm_kernel_buf(const void *state, void *output, unsigned int index);
int dovi_get_hdmi_metadata_buffer(const void *state, void *output);
void dovi_cb_content_info(unsigned char content, unsigned char white, unsigned char extra1,
                          unsigned char extra2);

#include "kernel_map_serialization.h"

#include "shared_data.h"
#define dovi_control26_debug (dovi_retained_section_1.enable_debug)
#define dovi_control26_content_type (dovi_retained_section_1.source_content_type[0])
#define dovi_control26_white_point (dovi_retained_section_1.desired_white_point[0])
#define dovi_control26_l11_byte2 (dovi_retained_section_1.level11_byte2[0])
#define dovi_control26_l11_byte3 (dovi_retained_section_1.level11_byte3[0])
#endif
