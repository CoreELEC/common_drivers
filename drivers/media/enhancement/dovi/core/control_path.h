/* Control processing with native settings and persistent storage. */
#ifndef DOVI_CONTROL_PATH_H
#define DOVI_CONTROL_PATH_H

#include "control_setting_layout.h"

/* Settings cover DOVI_SETTING_BYTES; input records use
 * DOVI_SETTING_INPUT_STRIDE. Persistent control state is shared across calls. */
int dovi_multi_control_path(int *setting);

#endif
