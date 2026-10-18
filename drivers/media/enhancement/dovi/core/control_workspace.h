#ifndef DOVI_CONTROL_WORKSPACE_H
#define DOVI_CONTROL_WORKSPACE_H
#include "control_setting_layout.h"

union dovi_four_source_words {
    unsigned long pairs[2];
    int words[4];
};

struct dovi_commit_workspace {
    union {
        unsigned char metadata[0x200];
        unsigned long primaries[4];
    } temporary;

    unsigned long decoded_headers[4];
    unsigned long metadata_configuration[20];
    union dovi_four_source_words source_flags;
    long metadata[4];
    union dovi_four_source_words metadata_sizes;
    unsigned char reserved_300[4];
    unsigned int primary_y[3];
    unsigned char reserved_310[0x70];
};

struct dovi_multi_workspace {
    unsigned char reserved_000[0x5c];
    unsigned int primary_core;
    unsigned char reserved_060[0xa0];

    union {
        unsigned char bytes[16];
        unsigned int words[4];
        unsigned long pairs[2];
    } input_configuration;

    unsigned int initialization[2];
    union dovi_hdr_infoframe infoframe;

    union {
        int words[30];
        unsigned long pairs[15];
    } target;

    int payload_sizes[16];
    long payloads[10];
    unsigned char reserved_240[0x60];
};

_Static_assert(__builtin_offsetof(struct dovi_commit_workspace, decoded_headers) == 0x200,
               "decoded header workspace offset");
_Static_assert(__builtin_offsetof(struct dovi_commit_workspace, source_flags) == 0x2c0,
               "source status workspace offset");
_Static_assert(__builtin_offsetof(struct dovi_commit_workspace, metadata) == 0x2d0,
               "source metadata workspace offset");
_Static_assert(sizeof(struct dovi_commit_workspace) == 0x380, "commit workspace size");
_Static_assert(__builtin_offsetof(struct dovi_multi_workspace, input_configuration) == 0x100,
               "input configuration workspace offset");
_Static_assert(__builtin_offsetof(struct dovi_multi_workspace, infoframe) == 0x118,
               "HDR infoframe workspace offset");
_Static_assert(__builtin_offsetof(struct dovi_multi_workspace, target) == 0x138,
               "target parameter workspace offset");
_Static_assert(__builtin_offsetof(struct dovi_multi_workspace, payload_sizes) == 0x1b0,
               "payload size workspace offset");
_Static_assert(__builtin_offsetof(struct dovi_multi_workspace, payloads) == 0x1f0,
               "payload pointer workspace offset");
_Static_assert(sizeof(struct dovi_multi_workspace) == 0x2a0, "multi-input workspace size");

#endif
