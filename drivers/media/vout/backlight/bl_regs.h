/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_BL_REGS_H__
#define __AML_BL_REGS_H__

#include <linux/amlogic/media/registers/register_map.h>

#define VPU_VPU_PWM_V0                             0x2730
#define VPU_VPU_PWM_V1                             0x2731
#define VPU_VPU_PWM_V2                             0x2732
#define VPU_VPU_PWM_V3                             0x2733
#define VPU_VPU_PWM_H0                             0x2734

#define VPU_VPU_PWM_V0_T7                          0x1ce0
#define VPU_VPU_PWM_V1_T7                          0x1ce1
#define VPU_VPU_PWM_V2_T7                          0x1ce2
#define VPU_VPU_PWM_V3_T7                          0x1ce3
#define VPU_VPU_PWM_H0_T7                          0x1ce4

#define VPU1_VPU_PWM_V0                            0x22e0
#define VPU1_VPU_PWM_V1                            0x22e1
#define VPU1_VPU_PWM_V2                            0x22e2
#define VPU1_VPU_PWM_V3                            0x22e3
#define VPU1_VPU_PWM_H0                            0x22e4
#define VPU1_VPU_PWM_H1                            0x22e5

#define VPU_VENC_CTRL                              0x1cef
#define VPP_INT_LINE_NUM                           0x1dce
#define VPU_DISP_VIU0_CTRL                         0x2786
#define VPU_DISP_VIU1_CTRL                         0x2787
#define VPU_DISP_VIU2_CTRL                         0x2788
#define VPU_VENC_RGN_CTRL                          0x2789
#define VPU_VENC_RGN_RSIZE                         0x278a
#define VPU_DISP_WRAP_CTRL                         0x278b

#define ENCL_VIDEO_MAX_LNCNT                       0x1cbb

// T6W bringup
//ambight
#define LDC_AMBIGHT_HSIZE                          0x14c6
#define LDC_AMBIGHT_VSIZE                          0x14c7
#define LDC_AMBIGHT_STAT_NUM                       0x14c8
//
// Closing file:  ./vencl_lcd_reg.h
//
//

static inline u32 bl_vcbus_read(u32 reg)
{
	u32 temp;
	// unsigned long flags = 0;

	// spin_lock_irqsave(&edptx_reg_spinlock, flags);
	temp = aml_read_vcbus(reg);
	// spin_unlock_irqrestore(&edptx_reg_spinlock, flags);

	return temp;
};

static inline void bl_vcbus_write(u32 reg, u32 value)
{
	// unsigned long flags = 0;

	// spin_lock_irqsave(&edptx_reg_spinlock, flags);
	aml_write_vcbus(reg, value);
	// spin_unlock_irqrestore(&edptx_reg_spinlock, flags);
};

static inline void bl_vcbus_setb(u32 reg, u32 value, u32 start, u32 len)
{
	// unsigned long flags = 0;

	// spin_lock_irqsave(&edptx_reg_spinlock, flags);
	aml_write_vcbus(reg, ((aml_read_vcbus(reg) &
		(~(((1L << len) - 1) << start))) |
		((value & ((1L << len) - 1)) << start)));
	// spin_unlock_irqrestore(&edptx_reg_spinlock, flags);
}

static inline u32 bl_vcbus_getb(u32 reg, u32 start, u32 len)
{
	u32 temp;
	// unsigned long flags = 0;

	// spin_lock_irqsave(&edptx_reg_spinlock, flags);
	temp = (aml_read_vcbus(reg) >> start) & ((1L << len) - 1);
	// spin_unlock_irqrestore(&edptx_reg_spinlock, flags);

	return temp;
}

#endif
