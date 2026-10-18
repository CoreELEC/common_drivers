/* Persistent control-state initialization and access. */
#ifndef DOVI_CONTROL_STATE_H
#define DOVI_CONTROL_STATE_H

typedef void (*dovi_content_info_callback)(unsigned char content, unsigned char white,
                                           unsigned char extra1, unsigned char extra2);

/* Optional content-information callback, initially null. */
extern dovi_content_info_callback dovi_content_info_cb_handle;

/* InitDm requires 0x1f938 bytes of control storage, and count pointer slots
 * when signed count is positive. Negative counts skip per-instance setup;
 * positive counts above four retain the original partial writes and trap. */
void *dovi_InitDm(int count, const void *key_slots, void *control);
/* The first argument to InitMdsExt is unused in the original ABI. */
void dovi_InitMdsExt(const void *configuration, void *extensions);
/* Initial memset covers 0x158 bytes; later stores extend through 0x1af. */
void dovi_InitTmCtrl(void *tone_control);
int dovi_GetStbTmmIdx(unsigned int index, void *min_pq, void *min, void *max_pq, void *max,
                      const void *control);
int dovi_destroy_cp(void);
void dovi_register_content_info_callback(dovi_content_info_callback callback);
void dovi_set_dovi2hdr10_mapping(void *context);
void dovi_set_force_maxCLL_FALL_to_0(void *state, unsigned int value);
int dovi_set_L11_hdmi_tx(void *state, unsigned int enabled, unsigned int content_type,
                         unsigned int intended_white, unsigned int additional_1,
                         unsigned int additional_2);
int dovi_set_i_dm_peak_stretch(void *state, unsigned int value);
void *dovi_get_dm_ks_buf(const void *state, unsigned int index);
void *dovi_get_dm_ctx_buf(const void *context);

int dovi_init_cp(void *arena, int instances, void *configuration);

#endif
