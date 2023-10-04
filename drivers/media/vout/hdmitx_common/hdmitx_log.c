// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/printk.h>
#include "hdmitx_log.h"

#define HDMITX_NAME		"hdmitx"

unsigned int __hdmitx_debug;
EXPORT_SYMBOL(__hdmitx_debug);

MODULE_PARM_DESC(hdmitx_debug, "Enable debug output, where each bit enables a debug category.\n"
"\t\tBit 0 (0x01)  will enable CORE messages (hdmitx core code)\n"
"\t\tBit 1 (0x02)  will enable VIDEO messages (hdmitx video)\n"
"\t\tBit 2 (0x04)  will enable AUDIO messages (hdmitx audio)\n"
"\t\tBit 3 (0x08)  will enable HDCP messages (hdmitx hdcp)\n"
"\t\tBit 4 (0x10)  will enable PACKET messages (hdmitx packet)\n"
"\t\tBit 5 (0x20)  will enable EDID messages (hdmitx edid parse)\n"
"\t\tBit 6 (0x40)  will enable PHY messages (hdmitx phy control)\n"
"\t\tBit 7 (0x80)  will enable REG messages (hdmitx register rd/wr)");
module_param_named(hdmitx_debug, __hdmitx_debug, int, 0600);

void __hdmitx_info(const char *format, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	pr_info("[" HDMITX_NAME ":] %pV", &vaf);

	va_end(args);
}
EXPORT_SYMBOL(__hdmitx_info);

void __hdmitx_err(const char *format, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	pr_err("[" HDMITX_NAME ":] *ERROR* %pV", &vaf);

	va_end(args);
}
EXPORT_SYMBOL(__hdmitx_err);

void __hdmitx_dbg(enum hdmitx_debug_category category, const char *format, ...)
{
	struct va_format vaf;
	va_list args;

	if (!hdmitx_debug_enabled(category))
		return;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	pr_info("[" HDMITX_NAME ":] %pV", &vaf);

	va_end(args);
}
EXPORT_SYMBOL(__hdmitx_dbg);

