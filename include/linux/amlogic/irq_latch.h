/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _IRQ_LATCH_H_
#define _IRQ_LATCH_H_
#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG)
void irq_latch_clr(int irq);
#else
static inline void irq_latch_clr(int irq) {};
#endif

#endif
