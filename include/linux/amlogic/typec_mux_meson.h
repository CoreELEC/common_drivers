/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __USB_TYPEC_MUX_MESON
#define __USB_TYPEC_MUX_MESON

#include <linux/notifier.h>

#define TYPEC_MESON_ORIENT 0x0
#define TYPEC_MESON_PIN_STATE 0x1
#define TYPEC_MESON_DP_STATE_DATA 0x2

#if IS_ENABLED(CONFIG_TYPEC_MUX_MESON)
void typec_mux_meson_register_notify(struct notifier_block *nb);
void typec_mux_meson_unregister_notify(struct notifier_block *nb);
#else
static inline void typec_mux_meson_register_notify(struct notifier_block *nb)
{
}

static inline void typec_mux_meson_unregister_notify(struct notifier_block *nb)
{
}
#endif /* CONFIG_TYPEC_MUX_MESON */

#endif /* __USB_TYPEC_MUX_MESON */
