#ifndef DOVI_CONTROL_PUBLISH_H
#define DOVI_CONTROL_PUBLISH_H
#include "control_setting_layout.h"

/* Length overflow reports the error and retains the previous payload bytes. */
void dovi_publish_dolby_metadata(void *output, const unsigned char *metadata);

#endif
