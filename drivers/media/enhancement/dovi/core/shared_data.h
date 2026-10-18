/* Canonical objects shared by the processing units. */
#ifndef DOVI_STB26_SHARED_DATA_H
#define DOVI_STB26_SHARED_DATA_H

#include "data_layout.h"
#include "control_state.h"

/* Canonical mutable objects owned by the recovered C units. */
extern dovi_content_info_callback dovi_content_info_cb_handle;
extern unsigned char dovi_DisplayIdxTbl[400];
extern unsigned int dovi_module_frame_nr;
extern const unsigned char dovi_DisplayTbl[140];
extern unsigned int dovi_Xyz2IctcpDm_m33Xyz2Lms[9];
extern unsigned int dovi_Xyz2IctcpDm_v3_0, dovi_Xyz2IctcpDm_v3_1;

#endif
