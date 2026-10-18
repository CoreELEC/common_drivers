/* Named storage for the shared control state and fixed-point tables.
 *
 * The processing routines still address some members by byte offset. Explicit
 * reserved spans keep packed state views contiguous. Constant tables are
 * addressed through named members; their unused mirrors are not retained.
 * Do not alter packed state layouts independently of their consumers. All
 * integer tables have the AArch64 little-endian layout.
 */
#ifndef DOVI_DATA_LAYOUT_H
#define DOVI_DATA_LAYOUT_H

struct dovi_diagnostic_strings {
    const char assertion_false[2];
    const char default_chip_name[3];
    const char string_format[3];
    const char module_init_message[66];
    const char version_signature_format[9];
    const char assertion_format[39];
    const char build_description[59];
    const char control_init_failure[74];
};

/* Slots mirrored by separately owned globals remain reserved in this view;
 * processing uses the canonical objects declared in shared_data.h.
 */
struct dovi_control_storage {
    unsigned long content_callback_slot;
    unsigned int enable_debug;
    unsigned char source_content_type[1];
    unsigned char reserved_0000d[3];
    unsigned char desired_white_point[1];
    unsigned char reserved_00011[3];
    unsigned char level11_byte2[1];
    unsigned char reserved_00015[3];
    unsigned char level11_byte3[1];
    unsigned char reserved_00019[7];
    unsigned char control_path_context[166168];
    unsigned int core1_registers[484];
    unsigned int core2_registers[86];
    unsigned int core3_registers[256];
    unsigned int output_dm_lut[5120];
    unsigned char context_initialized[1];
    unsigned char reserved_2e621[7];
    unsigned char source_parameters[640];
    unsigned char composer_metadata[3600];
    unsigned char dm_metadata[8192];
    unsigned char cli_parameters[7468];
    unsigned char hdmi_metadata[509];
    unsigned char reserved_335e1[7];
    unsigned char display_index_slot[400];
    unsigned int xyz_to_lms_slot[9];
    unsigned int ictcp_luma_slot;
    unsigned int ictcp_chroma_slot;
    unsigned int frame_number_slot;
};

struct dovi_signature_storage {
    char version_signature[512];
    char chip_name[20];
};

/* Read-only tables and diagnostic strings used by the processing units. */
struct dovi_constant_storage {
    const char version[20];
    const struct dovi_diagnostic_strings diagnostics;
    const unsigned int level2_luma_trim[4];
    const unsigned int level8_luma_trim[7];
    const unsigned int level8_chroma_trim[14];
    const unsigned int level2_chroma_trim[2];
    const unsigned int pulsar_gamma_to_linear[256];
    const unsigned int sdr_gamma_to_linear[256];
    const unsigned short log10_value[4096];
    const int log10_rate[4096];
    const unsigned int power10_value[10][1024];
    const unsigned char power10_scale[10];
    const unsigned int linear_to_pq_slopes[128];
    const unsigned int linear_to_pq_intercepts[128];
    const unsigned int linear_to_pq_positions[128];
    const unsigned int version8_primaries[20][8];
};
extern struct dovi_control_storage dovi_retained_section_1;
extern struct dovi_signature_storage dovi_retained_section_2;
extern const struct dovi_constant_storage dovi_retained_section_3;

#endif
