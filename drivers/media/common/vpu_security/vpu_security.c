// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/dma-mapping.h>

#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/ctype.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/clk.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/amlogic/iomap.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/amlogic/arm-smccc.h>

#include <linux/amlogic/media/registers/regs/viu_regs.h>
#include <linux/amlogic/media/vpu_secure/vpu_secure.h>
#include <linux/amlogic/media/video_sink/video_signal_notify.h>
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/registers/regs/vpp_regs.h>
#include <linux/amlogic/media/utils/vdec_reg.h>

#include "vpu_security.h"
#include "vpu_security_reg.h"

#define DEVICE_NAME "vpu_security"
#define CLASS_NAME  "vpu_security"

static struct vpu_security_device_info vpu_security_info;
static struct sec_dev_data_s sec_meson_dev;
static u32 secure_cfg;
static u32 log_level;
static u32 debug_value;
static int value_save[VPP_TOP_MAX] = {0,};

static struct vpu_sec_reg_s reg_v1[] = {
	{VIU_DATA_SEC, 1, 0, 1}, /* 00. OSD1 */
	{VIU_DATA_SEC, 1, 1, 1}, /* 01. OSD2 */
	{VIU_DATA_SEC, 1, 2, 1}, /* 02. VD1 */
	{VIU_DATA_SEC, 1, 3, 1}, /* 03. VD2 */
	{VIU_DATA_SEC, 1, 4, 1}, /* 04. OSD3 */
	{VIU_DATA_SEC, 1, 5, 1}, /* 05. VD AFBC, not used */
	{VIU_DATA_SEC, 1, 6, 1}, /* 06. DV */
	{VIU_DATA_SEC, 1, 7, 1}, /* 07. OSD AFBC */
	{VIU_DATA_SEC, 1, 8, 1}  /* 08. VPP_TOP */
};

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static struct vpu_sec_reg_s reg_v2[] = {
	{MALI_AFBCD_TOP_CTRL,  1, 17, 1}, /* 00. OSD1 */
	{MALI_AFBCD_TOP_CTRL,  1, 22, 1}, /* 01. OSD2 */
	{VPP_VD1_DSC_CTRL,     1, 6,  1}, /* 02. VD1 */
	{VPP_VD2_DSC_CTRL,     1, 6,  1}, /* 03. VD2 */
	{MALI_AFBCD1_TOP_CTRL, 1, 22, 1}, /* 04. OSD3 */
	{0,                    1, 0,  1}, /* 05. VD AFBC, not used */
	{VPP_VD1_DSC_CTRL,     1, 7,  1}, /* 06. DV */
	{MALI_AFBCD_TOP_CTRL,  1, 18, 1}, /* 07. OSD AFBC */
	{VIU_FRM_CTRL,	       1, 26, 1}, /* 08. VPP_TOP */
	{MALI_AFBCD2_TOP_CTRL, 1, 22, 1}, /* 09. OSD4 */
	{VPP_VD3_DSC_CTRL,     1, 6,  1}, /* 10. VD3 */
	{VIU1_FRM_CTRL,        1, 26, 1}, /* 11. VPP_TOP1 */
	{VIU2_FRM_CTRL,        1, 26, 1}  /* 12. VPP_TOP2 */
};

static struct vpu_sec_reg_s reg_v3[] = {
	{VIU_OSD1_PATH_CTRL,   1, 28, 1}, /* 00. OSD1 */
	{VIU_OSD2_PATH_CTRL,   1, 28, 1}, /* 01. OSD2 */
	{VIU_VD1_PATH_CTRL,    1, 28, 1}, /* 02. VD1 */
	{VIU_VD2_PATH_CTRL,    1, 28, 1}, /* 03. VD2 */
	{VIU_OSD3_PATH_CTRL,   1, 28, 1}, /* 04. OSD3 */
	{0,                    1,  0, 1}, /* 05. VD AFBC, not used */
	{VIU_VD1_PATH_CTRL,    1, 29, 1}, /* 06. DV */
	{VIU_OSD4_PATH_CTRL,   1, 28, 1}, /* 07. OSD AFBC */
	{VIU_FRM_CTRL,	       1, 26, 1}, /* 08. VPP_TOP */
	{0,                    1, 0,  1}, /* 09. OSD4, not used */
	{VIU_VD3_PATH_CTRL,    1, 28, 1}, /* 10. VD3 */
	{VIU_FRM_CTRL,         1, 26, 1}, /* 11. VPP_TOP1 */
	{VIU_FRM_CTRL,         1, 26, 1}  /* 12. VPP_TOP2 */
};

static struct vpu_sec_reg_s reg_v4[] = {
	{S5_VIU_OSD1_MISC,   1, 4, 1}, /* 00. OSD1 */
	{S5_VIU_OSD2_MISC,   1, 4, 1}, /* 01. OSD2 */
	{S5_VIU_VD1_MISC,    1, 4, 1}, /* 02. vd1 slice 0 */
	{S5_VIU_VD5_MISC,	 1, 4, 1}, /* 03. vd2 */
	{S5_VIU_OSD3_MISC,   1, 4, 1}, /* 04. OSD3 */
	{0,                  1,  0, 1}, /* 05. VD AFBC, not used */
	{0,                  1,  0, 1}, /* 06. DV, not used */
	{S5_VIU_OSD1_MISC,   1, 5, 1}, /* 07. MALI_AFBCD0 */
	{S5_VIU_OSD3_MISC,   1, 5, 1}, /* 08. MALI_AFBCD1 */
	{S5_VIU_OSD4_MISC,   1, 4, 1}, /* 09. OSD4 */
	{S5_VIU_VD2_MISC,    1, 4, 1}, /* 10. 02.1 vd1 slice 1 */
	{S5_VIU_VD3_MISC,    1, 4, 1}, /* 11. 02.1 vd1 slice 2 */
	{S5_VIU_VD4_MISC,    1, 4, 1}, /* 12. 02.1 vd1 slice 3 */
};

static struct vpu_sec_reg_s reg_v5[] = {
	{VIU_DATA_SEC,       1, 0, 1}, /* 00. OSD1 */
	{VIU_DATA_SEC,       1, 1, 1}, /* 01. OSD2 */
	{VIU_DATA_SEC,       1, 2, 1}, /* 02. VD1 */
	{VIU_DATA_SEC,       1, 3, 1}, /* 03. VD2 */
	{VIU_DATA_SEC,       1, 4, 1}, /* 04. OSD3 */
	{VIU_DATA_SEC,       1, 5, 1}, /* 05. VD AFBC, not used */
	{VIU_DATA_SEC,       1, 6, 1}, /* 06. DV */
	{VIU_DATA_SEC,       1, 7, 1}, /* 07. OSD AFBC */
	{VIU_DATA_SEC,       1, 8, 1}, /* 08. VPP_TOP */
	{0,                  1, 0, 1}, /* 09. OSD4, not used */
	{0,                  1, 0, 1}, /* 10. VD3, not used */
	{0,                  1, 0, 1}, /* 11. VPP_TOP1, not used */
	{0,                  1, 0, 1}, /* 12. VPP_TOP2, not used */
	{VPU_LUT_DMA_SEC_IN, 1, 0, 1}, /* 13. VD1 FG */
	{VPU_LUT_DMA_SEC_IN, 1, 1, 1}, /* 14. VD2 FG */
	{VPU_LUT_DMA_SEC_IN, 1, 2, 1}  /* 15. DI FG */
};

static struct vpu_sec_reg_s reg_v6[] = {
	{VIU_OSD1_PATH_CTRL,     1, 28, 1}, /* 00. OSD1 */
	{VIU_OSD2_PATH_CTRL,     1, 28, 1}, /* 01. OSD2 */
	{VIU_VD1_PATH_CTRL,      1, 28, 1}, /* 02. VD1 */
	{VIU_VD2_PATH_CTRL,      1, 28, 1}, /* 03. VD2 */
	{VIU_OSD3_PATH_CTRL,     1, 28, 1}, /* 04. OSD3 */
	{0,                      1,  0, 1}, /* 05. VD AFBC, not used */
	{VIU_VD1_PATH_CTRL,      1, 29, 1}, /* 06. DV */
	{VIU_OSD4_PATH_CTRL,     1, 28, 1}, /* 07. OSD AFBC */
	{VIU_FRM_CTRL,	         1, 26, 1}, /* 08. VPP_TOP */
	{0,                      1, 0,  1}, /* 09. OSD4, not used */
	{VIU_VD3_PATH_CTRL,      1, 28, 1}, /* 10. VD3 */
	{VIU_FRM_CTRL,           1, 26, 1}, /* 11. VPP_TOP1 */
	{VIU_FRM_CTRL,           1, 26, 1}, /* 12. VPP_TOP2 */
	{T6D_VPU_LUT_DMA_SEC_IN, 1,  0, 1}, /* 13. VD1 FG */
	{T6D_VPU_LUT_DMA_SEC_IN, 1,  1, 1}, /* 14. VD2 FG */
	{T6D_VPU_LUT_DMA_SEC_IN, 1,  2, 1}  /* 15. DI FG */
};

static struct sec_dev_data_s vpu_security_sc2 = {
	.version = VPU_SEC_V1,
};
#endif

static struct sec_dev_data_s vpu_security_s4 = {
	.version = VPU_SEC_V1,
};

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static struct sec_dev_data_s vpu_security_t7 = {
	.version = VPU_SEC_V2,
};

static struct sec_dev_data_s vpu_security_t3 = {
	.version = VPU_SEC_V3,
};

static struct sec_dev_data_s vpu_security_s5 = {
	.version = VPU_SEC_V4,
};

static struct sec_dev_data_s vpu_security_s7 = {
	.version = VPU_SEC_V5,
};

static struct sec_dev_data_s vpu_security_t6d = {
	.version = VPU_SEC_V6,
};
#endif

static const struct of_device_id vpu_security_dt_match[] = {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	{
		.compatible = "amlogic, meson-sc2, vpu_security",
		.data = &vpu_security_sc2,
	},
#endif
	{
		.compatible = "amlogic, meson-s4, vpu_security",
		.data = &vpu_security_s4,
	},
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	{
		.compatible = "amlogic, meson-t7, vpu_security",
		.data = &vpu_security_t7,
	},
	{
		.compatible = "amlogic, meson-t3, vpu_security",
		.data = &vpu_security_t3,
	},
	{
		.compatible = "amlogic, meson-s5, vpu_security",
		.data = &vpu_security_s5,
	},
	{
		.compatible = "amlogic, meson-s7, vpu_security",
		.data = &vpu_security_s7,
	},
	{
		.compatible = "amlogic, meson-t6d, vpu_security",
		.data = &vpu_security_t6d,
	},
#endif
	{}
};

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static int _aml_write_vcbus_bits(u32 reg,
			  const u32 value,
			  const u32 start,
			  const u32 len)
{
	aml_write_vcbus(reg, ((aml_read_vcbus(reg) &
		 ~(((1L << (len)) - 1) << (start))) |
		(((value) & ((1L << (len)) - 1)) << (start))));
	return 0;
}
#endif

static bool is_vpu_secure_support(void)
{
	return sec_meson_dev.version ? true : false;
}

static u32 vpu_secure_version(void)
{
	return sec_meson_dev.version;
}

bool get_secure_state(enum module_port_e port)
{
	bool secure_enable = false;

	switch (port) {
	case VD1_OUT:
		if (secure_cfg & VD1_INPUT_SECURE)
			secure_enable = true;
		break;
	case VD2_OUT:
		if (secure_cfg & VD2_INPUT_SECURE)
			secure_enable = true;
		break;
	case VD3_OUT:
		if (secure_cfg & VD3_INPUT_SECURE)
			secure_enable = true;
		break;
	case OSD1_VPP_OUT:
		if ((secure_cfg & OSD1_INPUT_SECURE) ||
		    (secure_cfg & OSD2_INPUT_SECURE) ||
		    (secure_cfg & OSD3_INPUT_SECURE) ||
		    (secure_cfg & OSD4_INPUT_SECURE))
			secure_enable = true;
		break;
	case OSD2_VPP_OUT:
		break;
	case POST_BLEND_OUT:
		if (secure_cfg)
			secure_enable = true;
		break;
	case VPP_OUT:
		if (secure_cfg & VPP_OUTPUT_SECURE)
			secure_enable = true;
		break;
	default:
		break;
	}
	return secure_enable;
}

static void secure_reg_update(struct vpu_secure_ins *ins,
			      struct vpu_sec_bit_s *change,
			      u32 vpp_index)
{
	enum vpu_security_version_e version;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	int i, en = 0, reg_val = 0;
	int reg_size = 0;
	struct vpu_sec_reg_s *reg_item = NULL;
#endif

	if (!is_vpu_secure_support())
		return;

	if (!ins->reg_wr_op[vpp_index]) {
		pr_err("%s, reg_wr_op is NULL\n", __func__);
		return;
	}

	version = vpu_secure_version();
	if (version == VPU_SEC_V1) {
		/* write all bits together */
		ins->reg_wr_op[vpp_index](reg_v1[0].reg,
					  change->current_val,
					  0, ARRAY_SIZE(reg_v1));
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	} else if (version >= VPU_SEC_V2 && version < VPU_SEC_MAX) {
		if (version == VPU_SEC_V2) {
			reg_size = ARRAY_SIZE(reg_v2);
			reg_item = &reg_v2[0];
		} else if (version == VPU_SEC_V3) {
			reg_size = ARRAY_SIZE(reg_v3);
			reg_item = &reg_v3[0];
		} else if (version == VPU_SEC_V4) {
			reg_size = ARRAY_SIZE(reg_v4);
			reg_item = &reg_v4[0];
		} else if (version == VPU_SEC_V5) {
			reg_size = ARRAY_SIZE(reg_v5);
			reg_item = &reg_v5[0];
		} else if (version == VPU_SEC_V6) {
			reg_size = ARRAY_SIZE(reg_v6);
			reg_item = &reg_v6[0];
		}
		if (log_level & 1)
			pr_info("line=%d, vpu secure bit 0x%x, reg_size=0x%x, bit_changed=0x%x\n",
				__LINE__, change->current_val, reg_size,
				change->bit_changed);

		/* work through the array and write bit(s) */
		for (i = 0; i < reg_size; i++) {
			if (BIT(i) & change->bit_changed) {
				en = BIT(i) & change->current_val;
				reg_val = en ? reg_item[i].en :
					  (!reg_item[i].en);

				if (!reg_item[i].reg)
					continue;
				if (log_level & 1)
					pr_info("line=%d, vpu secure bit 0x%x, en=%d, reg_val=0x%x, reg=0x%x\n",
						__LINE__, change->current_val, en,
						reg_val,
						reg_item[i].reg);
				if (reg_item[i].reg == VPU_LUT_DMA_SEC_IN ||
					reg_item[i].reg == T6D_VPU_LUT_DMA_SEC_IN)
					_aml_write_vcbus_bits(reg_item[i].reg,
							  reg_val,
							  reg_item[i].start,
							  reg_item[i].len);
				else
					ins->reg_wr_op[vpp_index](reg_item[i].reg,
								  reg_val,
								  reg_item[i].start,
								  reg_item[i].len);
			}
		}
#endif
	} else {
		pr_err("%s, wrong version\n", __func__);
		return;
	}
}

u32 set_vpu_module_security(struct vpu_secure_ins *ins,
			    enum secure_module_e module,
			    u32 secure_src, u32 vpp_index)
{
	static u32 osd_secure[VPP_TOP_MAX] = {0,};
	static u32 video_secure[VPP_TOP_MAX] = {0,};
	static bool osd_secure_en[VPP_TOP_MAX] = {0,};
	static bool video_secure_en[VPP_TOP_MAX] = {0,};
	u32 value = 0, version = 0;
	int secure_update = 0;
	struct vd_secure_info_s vd_secure[MAX_SECURE_OUT];
	bool vpp_top_en = 0;
	struct vpu_sec_bit_s change;
	u32 reg_vpp_index = vpp_index;

	version = vpu_secure_version();
	if (!is_vpu_secure_support())
		return 0;

	/* prevsync belongs to the screen corresponding to VPP_TOP,
	 * but when updating the register, the RDMA channel of prevsync needs to be used.
	 * Therefore, there are two variables: vpp_index and reg_vpp_index.
	 */
	if (vpp_index == VPP_PRE_VSYNC)
		vpp_index = VPP_TOP;

	switch (module) {
	case OSD_MODULE:
		if ((secure_src & OSD1_INPUT_SECURE) ||
		    (secure_src & OSD2_INPUT_SECURE) ||
		    (secure_src & OSD3_INPUT_SECURE) ||
		    (secure_src & OSD4_INPUT_SECURE) ||
		    (secure_src & MALI_AFBCD_SECURE)) {
			/* for T7 revA */
			if (is_meson_t7_cpu() && is_meson_rev_a() &&
			    (secure_src & (OSD1_INPUT_SECURE |
					   OSD2_INPUT_SECURE)))
				secure_src |= OSD4_INPUT_SECURE;

			/* OSD module secure */
			osd_secure[vpp_index] = secure_src;
			value = osd_secure[vpp_index] |
				video_secure[vpp_index];
			ins->secure_enable = 1;

			osd_secure_en[vpp_index] = 1;
		} else {
			/* OSD none secure */
			osd_secure[vpp_index] = secure_src;
			value = osd_secure[vpp_index] |
				video_secure[vpp_index];
			ins->secure_enable = 0;
			osd_secure_en[vpp_index] = 0;
		}
		if (version == VPU_SEC_V4) {
			u32 temp;

			if (value & VD1_INPUT_SECURE) {
				if (is_meson_t3x_cpu())
					temp = VD1_SLICE1_SECURE;
				else
					temp = VD1_SLICE1_SECURE |
						VD1_SLICE2_SECURE |
						VD1_SLICE3_SECURE;
				value |= temp;
				if (log_level & 4)
					pr_info("line=%d,secure_enable=%d module=%d value=0x%x\n",
						__LINE__, module, ins->secure_enable, value);
				}
		}
		ins->secure_status = value;
		break;
	case VIDEO_MODULE:
		if ((secure_src & VD2_FGRAIN_SECURE) ||
		    (secure_src & VD1_FGRAIN_SECURE) ||
		    (secure_src & DV_INPUT_SECURE) ||
		    (secure_src & AFBCD_INPUT_SECURE) ||
		    (secure_src & VD3_INPUT_SECURE) ||
		    (secure_src & VD2_INPUT_SECURE) ||
		    (secure_src & VD1_INPUT_SECURE)) {
			/* video module secure */
			video_secure[vpp_index] = secure_src;
			value = video_secure[vpp_index] |
				osd_secure[vpp_index];
			ins->secure_enable = 1;
			video_secure_en[vpp_index] = 1;
		} else {
			/* video module none secure */
			video_secure[vpp_index] = secure_src;
			value = video_secure[vpp_index] |
				osd_secure[vpp_index];
			ins->secure_enable = 0;
			video_secure_en[vpp_index] = 0;
		}
		if (version == VPU_SEC_V4) {
			u32 temp;

			if (value & VD1_INPUT_SECURE) {
				if (is_meson_t3x_cpu())
					temp = VD1_SLICE1_SECURE;
				else
					temp = VD1_SLICE1_SECURE |
						VD1_SLICE2_SECURE |
						VD1_SLICE3_SECURE;
				value |= temp;
				if (log_level & 4)
					pr_info("line=%d,secure_enable=%d module=%d value=0x%x\n",
						__LINE__, module, ins->secure_enable, value);
				}
		}
		ins->secure_status = value;
		break;
	case DI_MODULE:
		break;
	case VDIN_MODULE:
		break;
	default:
		break;
	}

	if (version < VPU_SEC_V4 || version == VPU_SEC_V5) {
		vpp_top_en = osd_secure_en[vpp_index] ||
			     video_secure_en[vpp_index];
		if (vpp_index == 0) {
			if (vpp_top_en)
				value |= VPP_OUTPUT_SECURE;
			else
				value &= ~VPP_OUTPUT_SECURE;
		}
		if (vpp_index == 1) {
			if (vpp_top_en)
				value |= VPP1_OUTPUT_SECURE;
			else
				value &= ~VPP1_OUTPUT_SECURE;
		}
		if (vpp_index == 2) {
			if (vpp_top_en)
				value |= VPP2_OUTPUT_SECURE;
			else
				value &= ~VPP2_OUTPUT_SECURE;
		}
	}
	/* debug value setting */
	if (debug_value)
		value = debug_value;

	if (module == OSD_MODULE ||
		module == VIDEO_MODULE ||
		module == DI_MODULE) {
		if (value_save[vpp_index] != value) {
			/* record changed bit and current val */
			change.bit_changed =
					value ^ value_save[vpp_index];
			change.current_val = value;
			if (log_level & 1)
				pr_info("line=%d,module=%d value=0x%x, value_save=0x%x, bit_changed=0x%x\n",
					__LINE__, module, value, value_save[vpp_index],
					change.bit_changed);
			secure_reg_update(ins, &change, reg_vpp_index);
			secure_update = 1;
		}
		value_save[vpp_index] = value;
	}

	if (log_level & 2)
		pr_info("module=%d ,vpu secure bit 0x%x\n",
			module, value);

	secure_cfg = value_save[VPP_TOP] |
			value_save[VPP_TOP_1] |
			value_save[VPP_TOP_2];
	if (secure_update) {
		int i;

		for (i = 0; i < MAX_SECURE_OUT; i++) {
			vd_secure[i].secure_type = i;
			vd_secure[i].secure_enable = get_secure_state(i);
		}
		vd_signal_notifier_call_chain
			(VIDEO_SECURE_TYPE_CHANGED,
			&vd_secure[0]);
	}
	return value;
}

int secure_register(enum secure_module_e module,
		    int config_delay,
		    void **reg_op,
		    void *cb)
{
	struct vpu_security_device_info *info = &vpu_security_info;
	struct vpu_secure_ins *ins = NULL;
	struct mutex *lock = NULL;
	int i;

	if (!is_vpu_secure_support())
		return -1;
	if (!info->probed)
		return -1;
	if (module >= MODULE_NUM) {
		pr_info("%s failed, module = %d\n", __func__, module);
		return -1;
	}
	ins = &info->ins[module];
	if (!ins->registered) {
		lock = &info->ins[module].secure_lock;
		mutex_lock(lock);
		ins->registered = 1;
		ins->config_delay = config_delay;
		for (i = 0; i < VPP_TOP_MAX; i++)
			ins->reg_wr_op[i] = reg_op[i];
		ins->secure_cb = cb;
		mutex_unlock(lock);
		pr_info("%s module=%d ok\n", __func__, module);
	}
	return 0;
}
EXPORT_SYMBOL(secure_register);

int secure_unregister(enum secure_module_e module)
{
	struct vpu_security_device_info *info = &vpu_security_info;
	struct vpu_secure_ins *ins = NULL;
	struct mutex *lock = NULL;
	int i;

	if (!is_vpu_secure_support())
		return -1;
	if (!info->probed)
		return -1;
	if (module >= MODULE_NUM)
		return -1;
	ins = &info->ins[module];
	if (ins->registered) {
		lock = &info->ins[module].secure_lock;
		mutex_lock(lock);
		ins->registered = 0;
		ins->config_delay = 0;
		for (i = 0; i < VPP_TOP_MAX; i++)
			ins->reg_wr_op[i] = NULL;
		ins->secure_cb = NULL;
		mutex_unlock(lock);
	}
	return 0;
}

int secure_config(enum secure_module_e module, int secure_src, u32 vpp_index)
{
	struct vpu_security_device_info *info = &vpu_security_info;
	struct vpu_secure_ins *ins = NULL;
	u32 reg_value = -1;

	if (module >= MODULE_NUM)
		return -1;
	ins = &info->ins[module];
	if (ins->registered)
		reg_value = set_vpu_module_security(ins, module, secure_src,
						    vpp_index);
	return 0;
}
EXPORT_SYMBOL(secure_config);

static ssize_t vpu_security_info_show(const struct class *cla,
				      const struct class_attribute *attr, char *buf)
{
	struct vpu_security_device_info *info = &vpu_security_info;

	return snprintf(buf, PAGE_SIZE, "vpu_security mismatch cnt:%d\n",
		info->mismatch_cnt);
}

static ssize_t log_level_show(const struct class *cla,
			      const struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 40, "%d\n", log_level);
}

static ssize_t log_level_store(const struct class *cla,
			       const struct class_attribute *attr,
			       const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	pr_info("log_level: %d->%d\n", log_level, res);
	log_level = res;
	return count;
}

static ssize_t debug_value_show(const struct class *cla,
			      const struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf + len, "cur val:0x%x\n", debug_value);
	len += sprintf(buf + len, "bit00. OSD1\n");
	len += sprintf(buf + len, "bit01. OSD2\n");
	len += sprintf(buf + len, "bit02. VD1\n");
	len += sprintf(buf + len, "bit03. VD2\n");
	len += sprintf(buf + len, "bit04. OSD3\n");
	len += sprintf(buf + len, "bit05. VD AFBC, not used\n");
	len += sprintf(buf + len, "bit06. DV\n");
	len += sprintf(buf + len, "bit07. OSD AFBC\n");
	len += sprintf(buf + len, "bit08. VPP_TOP\n");
	len += sprintf(buf + len, "bit09. OSD4\n");
	len += sprintf(buf + len, "bit10. VD3\n");
	len += sprintf(buf + len, "bit11. VPP_TOP1\n");
	len += sprintf(buf + len, "bit12. VPP_TOP2\n");
	len += sprintf(buf + len, "bit13. VD1_FGRAIN\n");
	len += sprintf(buf + len, "bit14. VD2_FGRAIN\n");
	len += sprintf(buf + len, "bit15. DI_FGRAIN\n");

	return len;
}

static ssize_t debug_value_store(const struct class *cla,
			       const struct class_attribute *attr,
			       const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	pr_info("debug_value: %d->%d\n", debug_value, res);
	debug_value = res;
	return count;
}

static struct class_attribute vpu_security_attrs[] = {
	__ATTR(security_info, 0444,
	       vpu_security_info_show, NULL),
	__ATTR(log_level, 0644,
	       log_level_show, log_level_store),
	__ATTR(debug_value, 0644,
	       debug_value_show, debug_value_store),
};

#ifdef USE_VPU_SECURIT_ISR
irqreturn_t vpu_security_isr(int irq, void *dev_id)
{
	struct vpu_security_device_info *info = &vpu_security_info;
	struct vpu_secure_ins *ins = NULL;
	int i;
	enum vpu_security_version_e version;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	u32 error_bit = 0;
#endif

	version = vpu_secure_version();

	info->mismatch_cnt++;
	if (version == VPU_SEC_V1) {
		if (log_level >= 1)
			pr_err("sec: mismatch(%d)\n", info->mismatch_cnt);
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	} else if (version == VPU_SEC_V2) {
		error_bit = READ_VCBUS_REG(VPU_SEC_INT_STAT);
		for (i = 0; i < VPP_TOP_MAX; i++) {
			if (error_bit & BIT(i)) {
				if (log_level >= 1)
					pr_err("sec: VPP_TOP%x mismatch\n", i);
				/* clear interrupt bit */
				WRITE_VCBUS_REG_BITS(VPU_SEC_INT_STAT, 1, i, 1);
			}
		}
#endif
	}

	for (i = 0; i < MODULE_NUM; i++) {
		ins = &info->ins[i];
		if (ins->registered) {
			if (ins->secure_cb)
				ins->secure_cb(ins->secure_status);
		}
	}
	return IRQ_HANDLED;
}
#endif

static int vpu_security_probe(struct platform_device *pdev)
{
	int i, ret = 0, err = 0;
#ifdef USE_VPU_SECURIT_ISR
	int int_vpu_security;
#endif
	struct vpu_security_device_info *info = &vpu_security_info;

	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		struct sec_dev_data_s *sec_meson;
		struct device_node	*of_node = pdev->dev.of_node;

		match = of_match_node(vpu_security_dt_match, of_node);
		if (match) {
			sec_meson =
				(struct sec_dev_data_s *)match->data;
			if (sec_meson)
				memcpy(&sec_meson_dev, sec_meson,
				       sizeof(struct sec_dev_data_s));
			else
				err = 1;

		} else {
			err = 2;
		}
	} else {
		err = 3;
	}
	if (err) {
		pr_err("dev %s NOT match, err=%d\n", __func__, err);
		return -ENODEV;
	}
#ifdef USE_VPU_SECURIT_ISR
	info->pdev = pdev;
	int_vpu_security = platform_get_irq_byname(pdev, "vpu_security");
	if (request_irq(int_vpu_security, &vpu_security_isr,
			IRQF_SHARED, "vpu_security", (void *)"vpu_security")) {
		dev_err(&pdev->dev, "can't request irq for vpu_security\n");
		return -ENODEV;
	}
#endif
	info->clsp = class_create(CLASS_NAME);
	if (IS_ERR(info->clsp)) {
		ret = PTR_ERR(info->clsp);
		pr_err("fail to create class\n");
		goto fail_create_class;
	}
	for (i = 0; i < ARRAY_SIZE(vpu_security_attrs); i++) {
		if (class_create_file
			(info->clsp,
			&vpu_security_attrs[i]) < 0) {
			pr_err("fail to class_create_file\n");
			goto fail_class_create_file;
		}
	}
	for (i = 0; i < MODULE_NUM; i++)
		mutex_init(&info->ins[i].secure_lock);

	info->probed = 1;
	pr_debug("%s ok.\n", __func__);
	return 0;
fail_class_create_file:
	for (i = 0; i < ARRAY_SIZE(vpu_security_attrs); i++)
		class_remove_file
			(info->clsp, &vpu_security_attrs[i]);
	class_destroy(info->clsp);
	info->clsp = NULL;
fail_create_class:
	return ret;
}

static void vpu_security_remove(struct platform_device *pdev)
{
	int i;
	struct vpu_security_device_info *info = &vpu_security_info;

	for (i = 0; i < ARRAY_SIZE(vpu_security_attrs); i++)
		class_remove_file
			(info->clsp, &vpu_security_attrs[i]);
	class_destroy(info->clsp);
	info->clsp = NULL;
	info->probed = 0;
}

#ifdef CONFIG_PM
static int vpu_security_suspend(struct device *dev)
{
	int i;

	/* to ensure that the registers inside secure_config() will be updated */
	for (i = 0; i < VPP_TOP_MAX; i++)
		value_save[i] = 0;
	pr_info("call %s\n", __func__);

	return 0;
}

static int vpu_security_resume(struct device *dev)
{
	pr_info("call %s\n", __func__);
	return 0;
}

static int vpu_security_freeze(struct device *dev)
{
	int i;

	for (i = 0; i < VPP_TOP_MAX; i++)
		value_save[i] = 0;
	pr_info("call %s\n", __func__);

	return 0;
}

static int vpu_security_thaw(struct device *dev)
{
	pr_info("call %s\n", __func__);
	return 0;
}

static int vpu_security_restore(struct device *dev)
{
	pr_info("call %s\n", __func__);
	return 0;
}

static const struct dev_pm_ops vpu_security_pm_ops = {
	.suspend = vpu_security_suspend,
	.resume  = vpu_security_resume,
	.freeze  = vpu_security_freeze,
	.thaw    = vpu_security_thaw,
	.restore = vpu_security_restore,
};
#endif

static struct platform_driver vpu_security_driver = {
	.probe = vpu_security_probe,
	.remove = vpu_security_remove,
	.driver = {
		.name = "amlogic_vpu_security",
		.of_match_table = vpu_security_dt_match,
#ifdef CONFIG_PM
		.pm = &vpu_security_pm_ops,
#endif
	},
};

int __init vpu_security_init(void)
{
	int r;

	r = platform_driver_register(&vpu_security_driver);
	if (r) {
		pr_err("Unable to register vpu_security driver\n");
		return r;
	}
	return 0;
}

void __exit vpu_security_exit(void)
{
	platform_driver_unregister(&vpu_security_driver);
}

//MODULE_DESCRIPTION("AMLOGIC VPU SECURITY driver");
//MODULE_LICENSE("GPL");
//MODULE_AUTHOR("PengCheng.Chen <pengcheng.chen@amlogic.com>");
