/* Byte offsets in the host control settings. */
#ifndef DOVI_CONTROL_SETTING_LAYOUT_H
#define DOVI_CONTROL_SETTING_LAYOUT_H

enum dovi_setting_offset {
    DOVI_SETTING_NUM_INPUT = 0x0,
    DOVI_SETTING_NUM_VIDEO = 0x4,
    DOVI_SETTING_PRI_INPUT = 0x8,
    DOVI_SETTING_ENABLE_MULTI_CORE1 = 0xc,
    DOVI_SETTING_ENABLE_DEBUG = 0x10,
    DOVI_SETTING_SET_PRIORITY = 0x14,
    DOVI_SETTING_DOVI2HDR10_NOMAPPING = 0x18,
    DOVI_SETTING_USE_LL_FLAG = 0x1c,
    DOVI_SETTING_LL_RGB_DESIRED = 0x20,
    DOVI_SETTING_VOUT_WIDTH = 0x24,
    DOVI_SETTING_VOUT_HEIGHT = 0x28,
    DOVI_SETTING_VSVDB_TBL = 0x2c,
    DOVI_SETTING_VSVDB_LEN = 0x4c,
    DOVI_SETTING_VSVDB_CHANGED = 0x50,
    DOVI_SETTING_MODE_CHANGED = 0x54,
    DOVI_SETTING_DST_FORMAT = 0x58,
    DOVI_SETTING_INPUT = 0x60,
    DOVI_SETTING_SET_GRAPHIC_MIN_LUM = 0x198,
    DOVI_SETTING_SET_GRAPHIC_MAX_LUM = 0x19c,
    DOVI_SETTING_SET_TARGET_MIN_LUM = 0x1a0,
    DOVI_SETTING_SET_TARGET_MAX_LUM = 0x1a4,
    DOVI_SETTING_CORE1 = 0x1a8,
    DOVI_SETTING_DM_REG2 = 0x2fe8,
    DOVI_SETTING_DM_REG3 = 0x3048,
    DOVI_SETTING_DM_LUT2 = 0x30b0,
    DOVI_SETTING_MD_REG3 = 0x44b0,
    DOVI_SETTING_HDR_INFO = 0x4cb4,
    DOVI_SETTING_DIAGNOSTIC_ENABLE = 0x4cd4,
    DOVI_SETTING_DIAGNOSTIC_MUX_SELECT = 0x4cd8,
    DOVI_SETTING_DOVI_LL_ENABLE = 0x4cdc,
    DOVI_SETTING_EXT_MD = 0x4ce0,
    DOVI_SETTING_OUTPUT_VSIF = 0x4d14,
    DOVI_SETTING_OUTPUT_CTRL_DATA = 0x4d40,
    DOVI_SETTING_OUTPUT_CTRL_DATA_LEN = 0x4d48,
    DOVI_SETTING_CTRL_DATA_TYPE = 0x4d4c,
    DOVI_SETTING_CONTENT_INFO = 0x4d50,
    DOVI_SETTING_RESERVED = 0x4d54,
    DOVI_SETTING_BYTES = 0x4f58,
    DOVI_SETTING_INPUT_STRIDE = 0x68,
};

enum dovi_input_offset {
    DOVI_INPUT_VALID = 0x0,
    DOVI_INPUT_SRC_FORMAT = 0x4,
    DOVI_INPUT_EL_FLAG = 0x8,
    DOVI_INPUT_EL_HALFSIZE_FLAG = 0x9,
    DOVI_INPUT_VIDEO_WIDTH = 0xc,
    DOVI_INPUT_VIDEO_HEIGHT = 0x10,
    DOVI_INPUT_SET_BIT_DEPTH = 0x14,
    DOVI_INPUT_SET_CHROMA_FORMAT = 0x18,
    DOVI_INPUT_SET_YUV_RANGE = 0x1c,
    DOVI_INPUT_COLOR_FORMAT = 0x20,
    DOVI_INPUT_IN_COMP = 0x28,
    DOVI_INPUT_IN_COMP_SIZE = 0x30,
    DOVI_INPUT_IN_MD = 0x38,
    DOVI_INPUT_IN_MD_SIZE = 0x40,
    DOVI_INPUT_VSEM_IF = 0x48,
    DOVI_INPUT_VSEM_IF_SIZE = 0x50,
    DOVI_INPUT_INPUT_MODE = 0x54,
    DOVI_INPUT_USE_PRIMARIES_FOR_DV = 0x58,
    DOVI_INPUT_P_HDR10_PARAM = 0x60,
};

/* Word indexing retains the original signed 32-bit loads and stores. */
#define DOVI_SETTING_WORD(offset) ((offset) / sizeof(int))
#define DOVI_INPUT_WORD(index, field) \
    DOVI_SETTING_WORD(DOVI_SETTING_INPUT + (index) * DOVI_SETTING_INPUT_STRIDE + (field))

/* HDMI static-metadata payload. Byte pairs stay separate where publication
 * updates them independently; the first word overlaps header and primaries. */
union dovi_hdr_infoframe {
    unsigned char bytes[32];

    struct __attribute__((packed)) {
        unsigned char header[5];
        unsigned short primary0_x;
        unsigned char primary0_y_lo, primary0_y_hi;
        unsigned short primary1_x, primary1_y, primary2_x;
        unsigned char primary2_y_lo, primary2_y_hi;
        unsigned short white_x, white_y;
        unsigned char max_luminance_lo, max_luminance_hi;
        unsigned char min_luminance_lo, min_luminance_hi;
        unsigned char max_content_light_lo, max_content_light_hi;
        unsigned char max_frame_average_lo, max_frame_average_hi;
    } fields;
};

_Static_assert(sizeof(((union dovi_hdr_infoframe *)0)->fields) == 29, "HDR static metadata length");
_Static_assert(__builtin_offsetof(union dovi_hdr_infoframe, fields.white_x) == 17,
               "HDR white point offset");
_Static_assert(__builtin_offsetof(union dovi_hdr_infoframe, fields.max_luminance_lo) == 21,
               "HDR luminance offset");

#endif
