/* Display-management metadata, configuration and kernel-state layouts. */
#ifndef DOVI_DM_LAYOUT_H
#define DOVI_DM_LAYOUT_H

/* Native metadata-change classifier extents and result bits. */
#define MDS_CHG_NONE (0u)
#define MDS_CHG_TC (4u)
#define MDS_CHG_CFG (1u)
#define DM_METADATA_COMPARE_BYTES 700
#define DM_METADATA_CONFIG_COMPARE_BYTES 120

enum {
    DM_SIGNAL_COUNT = 6,
    DM_SIGNAL_BYTES = 0x120,
    DM_CONTEXT_CONTROL_OFFSET = 0x8,
    DM_CONTROL_BYTES = 0x64,
    DM_CONTEXT_SIGNALS_OFFSET = 0x6c,
    DM_CONTEXT_TONE_CONTROL_OFFSET = 0x710,
    DM_TONE_CONTROL_BYTES = 0x1b0,
    DM_CONTEXT_GD_CONTROL_OFFSET = 0x8c0,
    DM_GD_CONTROL_BYTES = 0x20,
    DM_CONTEXT_GD_RESULT_OFFSET = 0x1f4d0,
    DM_CONTEXT_ACTIVE_AREA_OFFSET = 0x4430,
    DM_CONTEXT_USER_ADJUSTMENTS_OFFSET = 0x1f3d0,
    DM_CONTEXT_INPUT_STATE_STRIDE = 0x1f78,
    DM_CONTEXT_PRIMARY_SIGNAL_OFFSET = 0xc,
    DM_CONTEXT_PREVIOUS_MODE_OFFSET = 0x43b8,
    DM_CONTEXT_CURRENT_MODE_OFFSET = 0x43bc,
    DM_CONTEXT_TONE_DIRECTION_OFFSET = 0xe324,
};

struct dm_level11_control {
    int mode;
    int override;
    int content_type;
    int white_point;
    int reference_mode;
    int byte2;
    int byte3;
};

/* The configuration is copied into the context at byte 8. */
struct dm_control {
    int field_00;
    unsigned int primary_signal;
    int use_metadata;
    unsigned char fields_0c[0x14];
    int disable_output_metadata;
    int preserve_level4;
    unsigned int output_metadata_capacity;
    unsigned int output_metadata_mode;
    int field_30;
    struct dm_level11_control level11;
    unsigned char fields_50[0x14];
};

struct dm_context_header {
    unsigned int changes;
    int initialized;
    struct dm_control control;
};

/* The full signal record is 0x120 bytes. Fields without a confirmed meaning
 * retain their offset; they must not be borrowed from the older host ABI. */
struct dm_signal_environment {
    int signal_index;
    int field_04;
    int height;
    int width;
    int row_pitch;
    int chroma_row_pitch;
    int color_space;
    int chroma_format;
    int weave;
    int data_type;
    int bit_depth;
    int field_2c;
    int location;
    int field_34;
    int field_38;
    int primaries[8];
    int primaries_scale;
    unsigned char fields_60[0x94];
    int eotf;
    int gamma;
    int minimum;
    int maximum;
    int transfer_parameters[4];
    int field_114;
    int field_118;
    int field_11c;
};

struct dm_tone_control {
    unsigned char fields_00[0x14];
    unsigned int saturation_gain;
    unsigned int contrast;
    unsigned int chroma_weight;
    unsigned int white_point;
    unsigned int luminance_gain;
    unsigned char fields_28[DM_TONE_CONTROL_BYTES - 0x28];
};

/* There are five complete signal environments followed by the smaller alpha
 * environments. Index 5 in the legacy getter remains a byte-stride view. */
struct dm_configuration {
    struct dm_control control;
    struct dm_signal_environment signal[5];
    unsigned char alpha_environments[0x104];
    struct dm_tone_control tone_control;
    unsigned char global_dimming_control[DM_GD_CONTROL_BYTES];
    void *memory_manager;
    unsigned int field_8e0;
};

static inline struct dm_signal_environment *dm_config_signal(void *config, unsigned int index)
{
    return (void *)((unsigned char *)config + DM_CONTROL_BYTES +
                    (unsigned long)index * DM_SIGNAL_BYTES);
}

/* Coordinates are exclusive bottom/right endpoints after consolidation. */
struct dm_active_area {
    unsigned int enabled;
    unsigned int top;
    unsigned int left;
    unsigned int bottom;
    unsigned int right;
    unsigned int metadata_present;
    unsigned char fields_18[0x10];
};

struct dm_user_adjustments {
    int contrast;
    int saturation_gain;
    int chroma_weight;
    int luminance_gain;
    int white_point;
};

struct dm_trim_adjustments {
    int contrast_power;
    int contrast_slope;
    int chroma_weight;
    int saturation_gain;
    int luminance_gain;
};

struct dm_frame_format {
    unsigned int height;
    unsigned int width;
    unsigned char data_type;
    unsigned char weave;
    unsigned char location;
    unsigned char bit_depth;
    unsigned char chroma_format;
    unsigned char color_space;
    unsigned char fields_0e[2];
    unsigned int row_pitch;
    unsigned int column_pitch;
    unsigned int chroma_row_pitch;
};

/* Decoded metadata, rather than the big-endian wire representation. Matrix
 * coefficients and EOTF parameters retain their original fixed-point widths. */
struct dm_metadata_format {
    unsigned int flags;
    unsigned char version_major;
    unsigned char version_minor;
    unsigned short ycc_to_rgb_scale;
    unsigned short ycc_to_rgb[9];
    unsigned char fields_1a[2];
    unsigned int ycc_offsets[3];
    unsigned short rgb_to_lms_scale;
    unsigned short rgb_to_lms[9];
    unsigned short eotf;
    unsigned short eotf_scale;
    unsigned short eotf_offset;
    unsigned char fields_42[2];
    unsigned int eotf_range;
    unsigned char bit_depth;
    unsigned char color_space;
    unsigned char chroma_format;
    unsigned char full_range;
    unsigned short source_min_pq;
    unsigned short source_max_pq;
    unsigned short source_diagonal;
    unsigned short active_left;
    unsigned short active_right;
    unsigned short active_top;
    unsigned short active_bottom;
    unsigned char primaries_index;
    unsigned char field_5b;
    unsigned short primaries[8];
    unsigned char level11_white_point;
    unsigned char level11_content_type;
    unsigned char level11_reference_mode;
    unsigned char level254_mode;
    unsigned char level254_version;
    unsigned char run_mode;
    unsigned char run_version;
    unsigned char debug[4];
    unsigned char field_77;
};

struct dm_global_dimming_result {
    short active;
    unsigned short field_02;
    unsigned int model_input;
    unsigned int model_output;
    unsigned char fields_0c[0x18];
    unsigned int window_word_24;
    unsigned int window_word_28;
    unsigned char fields_2c[0xc];
    unsigned int filtered_linear;
    unsigned int filtered_pq;
};

#define DM_LAYOUT_OFFSET(type, field, offset) \
	_Static_assert(__builtin_offsetof(struct type, field) == (offset), \
		       #type "." #field " offset")
DM_LAYOUT_OFFSET(dm_control, primary_signal, 0x04);
DM_LAYOUT_OFFSET(dm_control, use_metadata, 0x08);
DM_LAYOUT_OFFSET(dm_control, disable_output_metadata, 0x20);
DM_LAYOUT_OFFSET(dm_control, preserve_level4, 0x24);
DM_LAYOUT_OFFSET(dm_control, output_metadata_capacity, 0x28);
DM_LAYOUT_OFFSET(dm_control, output_metadata_mode, 0x2c);
DM_LAYOUT_OFFSET(dm_control, level11, 0x34);
_Static_assert(sizeof(struct dm_control) == DM_CONTROL_BYTES, "DM control size");
DM_LAYOUT_OFFSET(dm_context_header, initialized, 4);
DM_LAYOUT_OFFSET(dm_context_header, control, DM_CONTEXT_CONTROL_OFFSET);
DM_LAYOUT_OFFSET(dm_configuration, signal, 0x64);
DM_LAYOUT_OFFSET(dm_configuration, alpha_environments, 0x604);
DM_LAYOUT_OFFSET(dm_configuration, tone_control, 0x708);
DM_LAYOUT_OFFSET(dm_configuration, global_dimming_control, 0x8b8);
DM_LAYOUT_OFFSET(dm_configuration, memory_manager, 0x8d8);
DM_LAYOUT_OFFSET(dm_configuration, field_8e0, 0x8e0);
DM_LAYOUT_OFFSET(dm_tone_control, saturation_gain, 0x14);
DM_LAYOUT_OFFSET(dm_tone_control, contrast, 0x18);
DM_LAYOUT_OFFSET(dm_tone_control, chroma_weight, 0x1c);
DM_LAYOUT_OFFSET(dm_tone_control, white_point, 0x20);
DM_LAYOUT_OFFSET(dm_tone_control, luminance_gain, 0x24);
_Static_assert(sizeof(struct dm_tone_control) == DM_TONE_CONTROL_BYTES, "tone control size");
DM_LAYOUT_OFFSET(dm_active_area, top, 4);
DM_LAYOUT_OFFSET(dm_active_area, left, 8);
DM_LAYOUT_OFFSET(dm_active_area, bottom, 0xc);
DM_LAYOUT_OFFSET(dm_active_area, right, 0x10);
DM_LAYOUT_OFFSET(dm_active_area, metadata_present, 0x14);
_Static_assert(sizeof(struct dm_active_area) == 0x28, "active area stride");
DM_LAYOUT_OFFSET(dm_user_adjustments, saturation_gain, 4);
DM_LAYOUT_OFFSET(dm_user_adjustments, chroma_weight, 8);
DM_LAYOUT_OFFSET(dm_user_adjustments, luminance_gain, 0xc);
DM_LAYOUT_OFFSET(dm_user_adjustments, white_point, 0x10);
DM_LAYOUT_OFFSET(dm_trim_adjustments, contrast_slope, 4);
DM_LAYOUT_OFFSET(dm_trim_adjustments, chroma_weight, 8);
DM_LAYOUT_OFFSET(dm_trim_adjustments, saturation_gain, 0xc);
DM_LAYOUT_OFFSET(dm_trim_adjustments, luminance_gain, 0x10);
DM_LAYOUT_OFFSET(dm_signal_environment, height, 0x8);
DM_LAYOUT_OFFSET(dm_signal_environment, color_space, 0x18);
DM_LAYOUT_OFFSET(dm_signal_environment, weave, 0x20);
DM_LAYOUT_OFFSET(dm_signal_environment, data_type, 0x24);
DM_LAYOUT_OFFSET(dm_signal_environment, bit_depth, 0x28);
DM_LAYOUT_OFFSET(dm_signal_environment, location, 0x30);
DM_LAYOUT_OFFSET(dm_signal_environment, primaries, 0x3c);
DM_LAYOUT_OFFSET(dm_signal_environment, primaries_scale, 0x5c);
DM_LAYOUT_OFFSET(dm_signal_environment, eotf, 0xf4);
DM_LAYOUT_OFFSET(dm_signal_environment, maximum, 0x100);
DM_LAYOUT_OFFSET(dm_signal_environment, transfer_parameters, 0x104);
DM_LAYOUT_OFFSET(dm_signal_environment, field_11c, 0x11c);
_Static_assert(sizeof(struct dm_signal_environment) == DM_SIGNAL_BYTES, "signal environment size");
DM_LAYOUT_OFFSET(dm_frame_format, data_type, 0x8);
DM_LAYOUT_OFFSET(dm_frame_format, bit_depth, 0xb);
DM_LAYOUT_OFFSET(dm_frame_format, row_pitch, 0x10);
DM_LAYOUT_OFFSET(dm_frame_format, column_pitch, 0x14);
DM_LAYOUT_OFFSET(dm_frame_format, chroma_row_pitch, 0x18);
_Static_assert(sizeof(struct dm_frame_format) == 0x1c, "frame format size");
DM_LAYOUT_OFFSET(dm_metadata_format, version_major, 0x4);
DM_LAYOUT_OFFSET(dm_metadata_format, ycc_to_rgb_scale, 0x6);
DM_LAYOUT_OFFSET(dm_metadata_format, ycc_to_rgb, 0x8);
DM_LAYOUT_OFFSET(dm_metadata_format, ycc_offsets, 0x1c);
DM_LAYOUT_OFFSET(dm_metadata_format, rgb_to_lms_scale, 0x28);
DM_LAYOUT_OFFSET(dm_metadata_format, rgb_to_lms, 0x2a);
DM_LAYOUT_OFFSET(dm_metadata_format, eotf, 0x3c);
DM_LAYOUT_OFFSET(dm_metadata_format, eotf_range, 0x44);
DM_LAYOUT_OFFSET(dm_metadata_format, bit_depth, 0x48);
DM_LAYOUT_OFFSET(dm_metadata_format, chroma_format, 0x4a);
DM_LAYOUT_OFFSET(dm_metadata_format, source_min_pq, 0x4c);
DM_LAYOUT_OFFSET(dm_metadata_format, source_diagonal, 0x50);
DM_LAYOUT_OFFSET(dm_metadata_format, active_left, 0x52);
DM_LAYOUT_OFFSET(dm_metadata_format, active_top, 0x56);
DM_LAYOUT_OFFSET(dm_metadata_format, primaries_index, 0x5a);
DM_LAYOUT_OFFSET(dm_metadata_format, primaries, 0x5c);
DM_LAYOUT_OFFSET(dm_metadata_format, level11_white_point, 0x6c);
DM_LAYOUT_OFFSET(dm_metadata_format, level254_mode, 0x6f);
DM_LAYOUT_OFFSET(dm_metadata_format, run_mode, 0x71);
DM_LAYOUT_OFFSET(dm_metadata_format, debug, 0x73);
_Static_assert(sizeof(struct dm_metadata_format) == 0x78, "metadata base size");
DM_LAYOUT_OFFSET(dm_global_dimming_result, model_input, 0x4);
DM_LAYOUT_OFFSET(dm_global_dimming_result, model_output, 0x8);
DM_LAYOUT_OFFSET(dm_global_dimming_result, window_word_24, 0x24);
DM_LAYOUT_OFFSET(dm_global_dimming_result, window_word_28, 0x28);
DM_LAYOUT_OFFSET(dm_global_dimming_result, filtered_linear, 0x38);
DM_LAYOUT_OFFSET(dm_global_dimming_result, filtered_pq, 0x3c);
#undef DM_LAYOUT_OFFSET

#endif
