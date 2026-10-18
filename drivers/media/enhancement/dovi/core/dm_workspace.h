/* Local matrix and trim storage shared by the display-management stages. */
#ifndef DOVI_DM_WORKSPACE_H
#define DOVI_DM_WORKSPACE_H

union dm_vector_workspace {
    unsigned long pairs[2];
    unsigned int words[4];
};

union dm_short_matrix_workspace {
    unsigned long pairs[3];
    short coefficients[12];
};

union dm_primaries_workspace {
    unsigned long pairs[4];
    int coordinates[8];
    unsigned long *trim;
};

union dm_matrix_workspace {
    unsigned long pairs[5];
    int coefficients[10];
};

struct dm_consolidation_workspace {
    unsigned char reserved_000[16];
    union dm_vector_workspace diagonal;
    union dm_vector_workspace white_xyz;
    unsigned long white_xy;
    union dm_short_matrix_workspace color_matrix;
    union dm_primaries_workspace target;
    union dm_primaries_workspace source;
    union dm_matrix_workspace temperature;
    unsigned short weight;
    unsigned char reserved_0ba[6];
    union dm_matrix_workspace rgb;
    union dm_matrix_workspace xyz;
    long mapping;
    unsigned char reserved_118[104];
};

struct dm_output_workspace {
    unsigned char reserved_000[16];
    unsigned char *count;
    unsigned char reserved_018[44];
    unsigned int trim_count;

    union {
        unsigned long pairs[2];

        struct {
            unsigned int target;
            unsigned short minimum;
            unsigned short maximum;
            unsigned char parameters[8];
        } fields;
    } level10;

    union {
        unsigned long pairs[8];
        unsigned int indices[16];
    } trims;

    unsigned char reserved_098[136];
};

#define DM_WORKSPACE_OFFSET(type, field, offset) \
    _Static_assert(__builtin_offsetof(struct type, field) == (offset), #type "." #field)
DM_WORKSPACE_OFFSET(dm_consolidation_workspace, diagonal, 16);
DM_WORKSPACE_OFFSET(dm_consolidation_workspace, white_xyz, 32);
DM_WORKSPACE_OFFSET(dm_consolidation_workspace, white_xy, 48);
DM_WORKSPACE_OFFSET(dm_consolidation_workspace, color_matrix, 56);
DM_WORKSPACE_OFFSET(dm_consolidation_workspace, target, 80);
DM_WORKSPACE_OFFSET(dm_consolidation_workspace, source, 112);
DM_WORKSPACE_OFFSET(dm_consolidation_workspace, temperature, 144);
DM_WORKSPACE_OFFSET(dm_consolidation_workspace, weight, 184);
DM_WORKSPACE_OFFSET(dm_consolidation_workspace, rgb, 192);
DM_WORKSPACE_OFFSET(dm_consolidation_workspace, xyz, 232);
DM_WORKSPACE_OFFSET(dm_consolidation_workspace, mapping, 272);
_Static_assert(sizeof(struct dm_consolidation_workspace) == 384, "consolidation workspace size");
DM_WORKSPACE_OFFSET(dm_output_workspace, count, 16);
DM_WORKSPACE_OFFSET(dm_output_workspace, trim_count, 68);
DM_WORKSPACE_OFFSET(dm_output_workspace, level10, 72);
DM_WORKSPACE_OFFSET(dm_output_workspace, level10.fields.minimum, 76);
DM_WORKSPACE_OFFSET(dm_output_workspace, level10.fields.maximum, 78);
DM_WORKSPACE_OFFSET(dm_output_workspace, level10.fields.parameters, 80);
DM_WORKSPACE_OFFSET(dm_output_workspace, trims, 88);
_Static_assert(sizeof(struct dm_output_workspace) == 288, "metadata output workspace size");
#undef DM_WORKSPACE_OFFSET
#endif
