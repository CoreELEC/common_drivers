/* Persistent control-state initialization and access. */
#ifndef DOVI_CONTROL_STATE_H
#define DOVI_CONTROL_STATE_H

#include "control_memory.h"

/* The arena contains internal pointers. It must be 8-byte aligned and
 * cover 20040 bytes. dovi_init_cp does not clear it or initialize mmg.reqNum;
 * callers supply a valid memory manager before initialization. Runtime memory
 * diagnostics are required whenever reqNum > 31 can be reached. Logger/dump may
 * redirect h_dm/h_mds_ext to valid objects; dovi_init_cp reloads both fields. */
#define CONTROL_ARENA_BYTES 20040u
void *dovi_InitDm(const void *config, void *dm);
int dovi_init_cp(void *arena);
int dovi_destroy_cp(void);
void dovi_set_dovi2hdr10_mapping(void *context);
void dovi_set_support_el(void *context, int enabled);
/* Return storage immediately following the 4192-byte control prefix. */
void *dovi_get_dm_ctx_buf(const void *context);
void dovi_set_graphics_offset(void *context, int minimum, int maximum);

/* The cli_param buffer is supplied explicitly by the outer binding. */
void populate_graphics_param_body(void *graphics, const void *cli);

void dovi_InitTmCtrl(void *tone_control);
/* Configuration and extensions must be distinct; initialized extensions retain
 * pointers into their own trim arrays and must remain at that address. */
void dovi_InitMdsExt(const void *configuration, void *extensions);
/* Copy the 984-byte to-be configuration when its validity flag is nonzero,
 * otherwise the active configuration. Invalid/null handles or selected source
 * pointers return -1 with destination unchanged. Copy runs forward bytewise,
 * including when source/destination overlap; storage must cover every access. */
int dovi_GetDmCfg(void *config, const void *dm_handle);

/* Optional outputs are written in min_pq, min_linear, max_pq, max_linear
 * order. Invalid/null active handles return -1 with all outputs unchanged.
 * Linear values use the clamped PQ lookup/interpolation. */
int dovi_GetStbCc(unsigned short *min_pq, unsigned int *min_linear, unsigned short *max_pq,
                  unsigned int *max_linear, const void *dm_handle);

#endif
