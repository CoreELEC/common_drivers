#ifndef DOVI_TONE_PARAMETERS_H
#define DOVI_TONE_PARAMETERS_H
/* Keep each store and subsequent reload ordered when caller buffers alias. */
void dovi_curve_prepare_parameters(void *control, void *parameters);
void dovi_CopyDataFromTcCtrlToTC(unsigned int *parameters, long control);
#endif
