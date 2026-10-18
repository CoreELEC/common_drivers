#ifndef DOVI_TONE_CONTROLS_H
#define DOVI_TONE_CONTROLS_H
#include "tone_curve.h"

/* Input sizes: TmCtrl 168, DmCtrl 104, TcCtrl 168, TcSig 28,
 * TgtSigEnv 204, DmKsTMap 4104 bytes. Functions retain unrelated bytes. */
void update_tc_ctrl_level(const void *tone_mapping, void *tone_curve);
void tm_ctrl_to_tc_ctrl(const void *tone_mapping, const void *dm_control, void *tone_curve,
                        int graphics_source);
void target_signal_to_tc_ctrl(const void *target_signal, void *tone_curve);
void update_tc_signal_hlg_a(int minimum, int maximum, void *tone_signal);
#endif
