// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/printk.h>

#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_log.h>

static bool tx_debug_enabled(struct meson_tx_log *log)
{
	return unlikely(log->debug & log->category);
}

void __meson_tx_print(struct meson_tx_log *log, struct va_format *vaf)
{
	if (log->print_level == DEBUG_LEVEL && !tx_debug_enabled(log))
		return;

	switch (log->print_level) {
	case INFO_LEVEL:
		pr_info("[%s] %pV", log->driver_tag, vaf);
		break;
	case WARN_LEVEL:
		pr_err("[%s] *WARNING* %pV", log->driver_tag, vaf);
		break;
	case ERROR_LEVEL:
		pr_err("[%s] *ERROR* %pV", log->driver_tag, vaf);
		break;
	case DEBUG_LEVEL:
		pr_info("[%s] %pV", log->driver_tag, vaf);
		break;
	}
}

void meson_tx_info(struct meson_tx_log *log, const char *format, ...)
{
	struct va_format vaf;
	va_list args;
	struct meson_tx_log tx_log;

	if (!log) {
		memset(&tx_log, 0, sizeof(tx_log));
		tx_log.driver_tag = TX_NAME;
	} else {
		memcpy(&tx_log, log, sizeof(*log));
	}

	tx_log.print_level = INFO_LEVEL;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;
	__meson_tx_print(&tx_log, &vaf);
	va_end(args);
}

void meson_tx_err(struct meson_tx_log *log, const char *format, ...)
{
	struct va_format vaf;
	va_list args;
	struct meson_tx_log tx_log;

	memcpy(&tx_log, log, sizeof(*log));
	tx_log.print_level = ERROR_LEVEL;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;
	__meson_tx_print(&tx_log, &vaf);
	va_end(args);
}

void meson_tx_warn(struct meson_tx_log *log, const char *format, ...)
{
	struct va_format vaf;
	va_list args;
	struct meson_tx_log tx_log;

	memcpy(&tx_log, log, sizeof(*log));
	tx_log.print_level = WARN_LEVEL;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;
	__meson_tx_print(&tx_log, &vaf);
	va_end(args);
}

void meson_tx_dbg(struct meson_tx_log *log, int category, const char *format, ...)
{
	struct va_format vaf;
	va_list args;
	struct meson_tx_log tx_log;

	memcpy(&tx_log, log, sizeof(*log));
	tx_log.print_level = DEBUG_LEVEL;
	tx_log.category = category;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;
	__meson_tx_print(&tx_log, &vaf);
	va_end(args);
}

void meson_tx_log_init(struct meson_tx_log *log, const char *device_name)
{
	log->driver_tag = device_name;
}
