/* Fixed-layout local views for input/output maps and trim selection. */
#ifndef DOVI_DM_MAP_WORKSPACE_H
#define DOVI_DM_MAP_WORKSPACE_H

union dm_map_vector {
    unsigned long pairs[2];
    unsigned int words[4];
};

union dm_trim_coordinates {
    unsigned long pairs[9];
    unsigned int values[17];
};

union dm_trim_pair_list {
    unsigned long pairs[4];
    unsigned int values[8];
};

union dm_map_color_matrix {
    struct {
        unsigned long pairs[2];
        short last;
    } packed;

    short coefficients[9];
};

union dm_map_primaries_matrix {
    struct {
        unsigned long pairs[4];
        unsigned int last;
    } packed;

    int coefficients[9];
    int primaries[8];
};

struct dm_metadata_input_workspace {
    unsigned char reserved_000[16];

    union {
        struct {
            unsigned int words[4];
            unsigned short last_coefficient;
        } packed;

        short coefficients[9];
    } rgb_matrix;

    unsigned char reserved_024[76];
};

struct dm_ipt_input_workspace {
    unsigned char reserved_000[20];
    int normalization;
    union dm_map_vector offsets;
    unsigned char reserved_028[72];
};

struct dm_signal_input_workspace {
    unsigned char reserved_000[20];
    unsigned int gamma[3];
    union dm_map_vector offsets;
    short rgb_to_lms[9];
    unsigned char reserved_042[78];
};

struct dm_signal_output_workspace {
    unsigned char reserved_000[16];

    union {
        unsigned long pair;
        unsigned int words[2];
    } offsets;

    unsigned int third_offset;
    unsigned char reserved_01c[4];
    short lms_to_rgb[9];
    unsigned char reserved_032[6];
    union dm_map_color_matrix crosstalk;
    long wide_matrix[9];
    unsigned char reserved_098[104];
};

struct dm_luma_coordinates_workspace {
    unsigned char reserved_000[24];
    long level10;
    unsigned char reserved_020[16];
    union dm_trim_coordinates upper;
    union dm_trim_coordinates rounded;
    unsigned char reserved_0c0[112];
};

struct dm_chroma_coordinates_workspace {
    unsigned char reserved_000[16];
    long level10;
    unsigned char reserved_018[24];
    union dm_trim_coordinates upper;
    union dm_trim_coordinates rounded;
    union dm_map_primaries_matrix color;

    union {
        unsigned long pair;
        int components[3];
    } ictcp;

    long xyz[3];
    unsigned char reserved_110[112];
};

struct dm_trim_selection_workspace {
    unsigned char reserved_000[16];

    union {
        unsigned long pair;

        struct {
            unsigned int target;
            unsigned int last_index;
        } fields;
    } selection;

    union {
        unsigned long pairs[2];
        unsigned int indices[4];
    } kept;
    union dm_trim_pair_list coordinates;
    union dm_trim_pair_list indices;
    unsigned char reserved_068[104];
};

struct dm_commit_workspace {
    unsigned char reserved_000[20];
    unsigned int flags;
    unsigned char reserved_018[104];
};

#define DM_MAP_OFFSET(type, field, offset) \
    _Static_assert(__builtin_offsetof(struct type, field) == (offset), #type "." #field)
DM_MAP_OFFSET(dm_metadata_input_workspace, rgb_matrix, 16);
DM_MAP_OFFSET(dm_metadata_input_workspace, rgb_matrix.packed.last_coefficient, 32);
DM_MAP_OFFSET(dm_ipt_input_workspace, normalization, 20);
DM_MAP_OFFSET(dm_ipt_input_workspace, offsets, 24);
DM_MAP_OFFSET(dm_signal_input_workspace, gamma, 20);
DM_MAP_OFFSET(dm_signal_input_workspace, offsets, 32);
DM_MAP_OFFSET(dm_signal_input_workspace, rgb_to_lms, 48);
DM_MAP_OFFSET(dm_signal_output_workspace, offsets, 16);
DM_MAP_OFFSET(dm_signal_output_workspace, third_offset, 24);
DM_MAP_OFFSET(dm_signal_output_workspace, lms_to_rgb, 32);
DM_MAP_OFFSET(dm_signal_output_workspace, crosstalk, 56);
DM_MAP_OFFSET(dm_signal_output_workspace, wide_matrix, 80);
DM_MAP_OFFSET(dm_luma_coordinates_workspace, level10, 24);
DM_MAP_OFFSET(dm_luma_coordinates_workspace, upper, 48);
DM_MAP_OFFSET(dm_luma_coordinates_workspace, rounded, 120);
DM_MAP_OFFSET(dm_chroma_coordinates_workspace, level10, 16);
DM_MAP_OFFSET(dm_chroma_coordinates_workspace, upper, 48);
DM_MAP_OFFSET(dm_chroma_coordinates_workspace, rounded, 120);
DM_MAP_OFFSET(dm_chroma_coordinates_workspace, color, 192);
DM_MAP_OFFSET(dm_chroma_coordinates_workspace, ictcp, 232);
DM_MAP_OFFSET(dm_chroma_coordinates_workspace, xyz, 248);
DM_MAP_OFFSET(dm_trim_selection_workspace, selection, 16);
DM_MAP_OFFSET(dm_trim_selection_workspace, kept, 24);
DM_MAP_OFFSET(dm_trim_selection_workspace, coordinates, 40);
DM_MAP_OFFSET(dm_trim_selection_workspace, indices, 72);
DM_MAP_OFFSET(dm_commit_workspace, flags, 20);
_Static_assert(sizeof(struct dm_metadata_input_workspace) == 112, "metadata input workspace size");
_Static_assert(sizeof(struct dm_ipt_input_workspace) == 112, "IPT input workspace size");
_Static_assert(sizeof(struct dm_signal_input_workspace) == 144, "signal input workspace size");
_Static_assert(sizeof(struct dm_signal_output_workspace) == 256, "signal output workspace size");
_Static_assert(sizeof(struct dm_luma_coordinates_workspace) == 304,
               "luma coordinate workspace size");
_Static_assert(sizeof(struct dm_chroma_coordinates_workspace) == 384,
               "chroma coordinate workspace size");
_Static_assert(sizeof(struct dm_trim_selection_workspace) == 208, "trim selection workspace size");
_Static_assert(sizeof(struct dm_commit_workspace) == 128, "commit workspace size");
#undef DM_MAP_OFFSET
#endif
