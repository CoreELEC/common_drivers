// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/pm.h>
#include <linux/suspend.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/psci.h>
#include <linux/errno.h>
#include <asm/suspend.h>
#include <linux/of_address.h>
#include <linux/input.h>
#include <linux/cpuidle.h>
#include <asm/cpuidle.h>
#include <uapi/linux/psci.h>
#include <linux/amlogic/arm-smccc.h>
#include <linux/amlogic/pm.h>
#include <linux/kobject.h>
#include <power.h>
#include <linux/amlogic/power_domain.h>
#include <linux/syscore_ops.h>
#include <linux/amlogic/gki_module.h>
#include <dt-bindings/power/meson-pm.h>
#include <linux/kdebug.h>
#include <linux/panic_notifier.h>
#include "main.h"

static u32 deep_mode;
static int deep_mode_init_done;
bool is_clr_resume_reason;

static unsigned int suspend_mode;
#define SMCID_POWER_MANAGER	0x82000079
#define SMCSUBID_PM_DDR_ASR	0x1000
#define SMCSUBID_PM_DUMP_INFO	0x1001
#define SMCSUBID_PM_DEEPCTRL	0x1002
#define SMCSUBID_PM_GET_EE_STAT	0x1003

#if IS_ENABLED(CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND)

static DEFINE_MUTEX(early_suspend_lock);
static DEFINE_MUTEX(sysfs_trigger_lock);
static LIST_HEAD(early_suspend_handlers);

/* In order to handle legacy early_suspend driver,
 * here we export sysfs interface
 * for user space to write /sys/power/early_suspend_trigger to trigger
 * early_suspend/late resume call back. If user space do not trigger
 * early_suspend/late_resume, this op will be done
 * by PM_SUSPEND_PREPARE notify.
 */
unsigned int sysfs_trigger;
unsigned int early_suspend_state;
/*
 * Avoid run early_suspend/late_resume repeatedly.
 */
unsigned int already_early_suspend;
/* early suspend debug flag */
unsigned int early_suspend_debug;
/* kernel & bl31 & bl30 suspend debug flag */
unsigned int suspend_debug_flag;
/* deep suspend enter flag */
static unsigned int deep_suspend;
/* get power event from input */
static int power_event_available;
/* get device name which send power event */
static char dev_name_event[128];
#define DRIVER_NAME "GX_PM"

void register_early_suspend(struct early_suspend *handler)
{
	struct list_head *pos;

	mutex_lock(&early_suspend_lock);
	list_for_each(pos, &early_suspend_handlers) {
		struct early_suspend *e;

		e = list_entry(pos, struct early_suspend, link);
		if (e->level > handler->level)
			break;
	}
	list_add_tail(&handler->link, pos);
	mutex_unlock(&early_suspend_lock);
}
EXPORT_SYMBOL(register_early_suspend);

void unregister_early_suspend(struct early_suspend *handler)
{
	mutex_lock(&early_suspend_lock);
	list_del(&handler->link);
	mutex_unlock(&early_suspend_lock);
}
EXPORT_SYMBOL(unregister_early_suspend);

static int suspend_get_pm_env(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (kstrtoint(buf, 0, &early_suspend_debug)) {
		pr_err("early_suspend_debug error: %s\n", buf);
		return -EINVAL;
	}

	return 1;
}
__setup("initcall_debug=", suspend_get_pm_env);

static inline void early_suspend(void)
{
	struct early_suspend *pos;

	mutex_lock(&early_suspend_lock);

	if (!already_early_suspend)
		already_early_suspend = 1;
	else
		goto end_early_suspend;
	//set node to 0 notify other drivers begin early suspend
	deep_suspend = 0;
	if (early_suspend_debug)
		pr_info("%s: call handlers\n", __func__);
	list_for_each_entry(pos, &early_suspend_handlers, link)
		if (pos->suspend) {
			if (early_suspend_debug)
				pr_info("%s: %ps\n", __func__, pos->suspend);
			pos->suspend(pos);
		}

	if (early_suspend_debug)
		pr_info("%s: done\n", __func__);

end_early_suspend:
	mutex_unlock(&early_suspend_lock);
}

static inline void late_resume(void)
{
	struct early_suspend *pos;

	mutex_lock(&early_suspend_lock);

	if (already_early_suspend)
		already_early_suspend = 0;
	else
		goto end_late_resume;

	if (early_suspend_debug)
		pr_info("%s: call handlers\n", __func__);
	list_for_each_entry_reverse(pos, &early_suspend_handlers, link)
		if (pos->resume) {
			if (early_suspend_debug)
				pr_info("%s: %ps\n", __func__, pos->resume);
			pos->resume(pos);
		}
	if (early_suspend_debug)
		pr_info("%s: done\n", __func__);

end_late_resume:
	mutex_unlock(&early_suspend_lock);
}

static ssize_t early_suspend_trigger_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	unsigned int len;

	len = sprintf(buf, "%d\n", early_suspend_state);

	return len;
}

static ssize_t early_suspend_trigger_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	int ret;

	ret = kstrtouint(buf, 0, &early_suspend_state);
	pr_info("early_suspend_state=%d\n", early_suspend_state);

	if (ret)
		return -EINVAL;

	mutex_lock(&sysfs_trigger_lock);
	sysfs_trigger = 1;

	if (early_suspend_state == 0)
		late_resume();
	else if (early_suspend_state == 1)
		early_suspend();
	mutex_unlock(&sysfs_trigger_lock);

	return count;
}

static CLASS_ATTR_RW(early_suspend_trigger);

#define SUSPEND_DEBUG_LOGLEVEL (0xf << (0))
#define SUSPEND_DEBUG_INITCALL_DEBUG (1U << (4))
static unsigned long __invoke_psci_fn_smc(unsigned long function_id,
					  unsigned long arg0,
					  unsigned long arg1,
					  unsigned long arg2);

static const char *suspend_debug_help_str = {
	"Usage:\n"
	"echo 0xXXXXXXXX > /sys/class/meson_pm/suspend_debug  //enable suspend debug mode\n\n"
	"Suspend Debug State List:\n\n"
	"--------------------------------------------------------------------------------------------------\n"
};

#define MAX_ENTRIES 32

struct Suspend_Debug_Mode {
	const char *description;
	int end_bit;
	int start_bit;
	int state;
};

struct Suspend_Debug_Mode suspend_debug_mode[MAX_ENTRIES] = {
	{"[BL30] clk/pll status check and show clk gate setting", 31, 31, 0},
	{"[BL30] mask RTC Wakeup source", 30, 30, 0},
	{"[BL30] mask SARADC Wakeup source", 29, 29, 0},
	{"[BL30] mask IR Wakeup source", 28, 28, 0},
	{"[BL30] show pwm state and voltage", 27, 27, 0},
	{"[BL30] function call log", 26, 26, 0},
	{"[BL30] open dmc monitor log", 25, 25, 0},
	{"[BL30] bl30 dump boot cpu's fsm and dsu's fsm", 24, 24, 0},
	{"[BL30] aocpu do ddr access(reserved)", 23, 23, 0},
	{"[BL30] skip power switch(vddee vddcpu vcc5v vcc3.3V etc)", 22, 22, 0},
	{"[BL30] skip ddr suspend in bl30", 21, 21, 0},
	{"[BL30] start debug task", 20, 20, 0},

	{"[BL31] reserved", 19, 17, 0},
	{"[BL31] skip clk gate switch", 16, 16, 0},
	{"[BL31] dump nonboot cpu's fsm state", 15, 15, 0},
	{"[BL31] skip ddr suspend in tlpm", 14, 14, 0},
	{"[BL31] power domain clk/pll status check and show clk gate setting", 13, 13, 0},
	{"[BL31] do clk reg check and rate check", 12, 12, 0},
	{"[BL31] do bl31 ddr checksum in tlpm", 11, 11, 0},
	{"[BL31] open dmc monitor log", 10, 10, 0},
	{"[BL31] bl31 debug loglevel(0:without log;1: normal;2: function call log)", 9, 8, 0},

	{"[KERNEL] reserved for dmc monitor", 7, 4, 0},
	{"[KERNEL] set kernel printk loglevel to 0~9", 3, 0, 0},
};

static void check_suspend_debug_mode(void)
{
	int i;

	for (i = 0; i < MAX_ENTRIES; i++) {
		int mask = (1 << (suspend_debug_mode[i].end_bit + 1 -
				  suspend_debug_mode[i].start_bit)) - 1;
		int value = (suspend_debug_flag >> suspend_debug_mode[i].start_bit) & mask;

		suspend_debug_mode[i].state = value;
	}
}

static void set_suspend_debug_flag(int suspend_flag)
{
	__invoke_psci_fn_smc(0x820000F3, 0, suspend_debug_flag &
		(~(SUSPEND_DEBUG_LOGLEVEL | SUSPEND_DEBUG_INITCALL_DEBUG)),
		  0);
}

static ssize_t suspend_debug_show(const struct class *class,
					  const struct class_attribute *attr,
					  char *buf)
{
	unsigned int len;
	int i;

	len = sprintf(&buf[0], "suspend_debug_flag = 0x%x\n\n", suspend_debug_flag);

	len += sprintf(&buf[len], "%s\n", suspend_debug_help_str);

	check_suspend_debug_mode();

	for (i = 0; i < MAX_ENTRIES; i++) {
		if (suspend_debug_mode[i].description) {
			if (suspend_debug_mode[i].start_bit == suspend_debug_mode[i].end_bit)
				len += sprintf(&buf[len], "%-80s bit[%d] \t%d\n",
					  suspend_debug_mode[i].description,
					  suspend_debug_mode[i].start_bit,
					  suspend_debug_mode[i].state);
			else
				len += sprintf(&buf[len], "%-80s bit[%d:%d] \t%d\n",
					  suspend_debug_mode[i].description,
					  suspend_debug_mode[i].end_bit,
					  suspend_debug_mode[i].start_bit,
					  suspend_debug_mode[i].state);
		}
	}

	return len;
}

static ssize_t suspend_debug_store(const struct class *class,
					   const struct class_attribute *attr,
					   const char *buf, size_t count)
{
	int ret;

	ret = kstrtouint(buf, 0, &suspend_debug_flag);
	pr_info("suspend_debug_flag = 0x%x\n", suspend_debug_flag);

	if (ret)
		return -EINVAL;

	set_suspend_debug_flag(suspend_debug_flag);

	return count;
}

static CLASS_ATTR_RW(suspend_debug);
static int suspend_get_debug_env(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (kstrtoint(buf, 0, &suspend_debug_flag)) {
		pr_err("suspend_debug_flag error: %s\n", buf);
		return -EINVAL;
	}

	return 1;
}

__setup("suspend_debug=", suspend_get_debug_env);

static void lgcy_early_suspend(void)
{
	mutex_lock(&sysfs_trigger_lock);

	if (!sysfs_trigger)
		early_suspend();

	mutex_unlock(&sysfs_trigger_lock);
}

static void lgcy_late_resume(void)
{
	mutex_lock(&sysfs_trigger_lock);

	if (!sysfs_trigger)
		late_resume();

	mutex_unlock(&sysfs_trigger_lock);
}

static int lgcy_early_suspend_notify(struct notifier_block *nb,
				     unsigned long event, void *dummy)
{
	if (event == PM_SUSPEND_PREPARE)
		lgcy_early_suspend();

	if (event == PM_POST_SUSPEND)
		lgcy_late_resume();

	return NOTIFY_OK;
}

static struct notifier_block lgcy_early_suspend_notifier = {
	.notifier_call = lgcy_early_suspend_notify,
};

static unsigned int lgcy_early_suspend_exit(struct platform_device *pdev)
{
	int ret;

	ret = unregister_pm_notifier(&lgcy_early_suspend_notifier);
	return ret;
}

#endif /*CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND*/

typedef unsigned long (psci_fn)(unsigned long, unsigned long,
				unsigned long, unsigned long);

static unsigned long __invoke_psci_fn_smc(unsigned long function_id,
					  unsigned long arg0,
					  unsigned long arg1,
					  unsigned long arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc(function_id, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	return res.a0;
}

static void __iomem *debug_reg;
static void __iomem *exit_reg;
static unsigned int resume_reason;
static unsigned int suspend_reason;
static unsigned int clear_suspend_reason;
static bool is_extd_resume_reason;

/*
 * get_resume_value return the register value that stores
 * resume reason.
 */
static uint32_t get_resume_value(void)
{
	u32 val = 0;

	if (exit_reg) {
		/* resume reason extension support for new soc such as s4/t3/sc2/s5... */
		if (is_extd_resume_reason)
			val = readl_relaxed(exit_reg) & 0xff;
		/* other soc such as tm2/axg do not support resume reason extension */
		else
			val = (readl_relaxed(exit_reg) >> 28) & 0xf;
	}

	return val;
}

/*
 * get_resume_reason always return last resume reason.
 */
unsigned int get_resume_reason(void)
{
	unsigned int val = 0;
	unsigned int reason;

	val = get_resume_value();
	if (is_extd_resume_reason)
		reason = val & 0x7f;
	else
		reason = val;

	return reason;
}
EXPORT_SYMBOL_GPL(get_resume_reason);

/*
 * get_resume_method return last resume reason.
 * It can be cleared by clr_resume_method().
 */
unsigned int get_resume_method(void)
{
	return resume_reason;
}
EXPORT_SYMBOL_GPL(get_resume_method);

/*
 * judge whether resume_type need power on lcd during resume.
 *
 */
bool is_screenon_resume_method(void)
{
	return (get_resume_method() != RTC_WAKEUP) &&
		(get_resume_method() != UNDEFINED_WAKEUP) &&
		(get_resume_method() != WIFI_WAKEUP) &&
		(get_resume_method() != ETH_PHY_WAKEUP);
}
EXPORT_SYMBOL_GPL(is_screenon_resume_method);
static void set_resume_method(unsigned int val)
{
	resume_reason = val;
}

static void clr_deep_mode(void)
{
	if (deep_mode_init_done)
		__invoke_psci_fn_smc(SMCID_POWER_MANAGER,
				SMCSUBID_PM_DEEPCTRL, SYSTEM_COLD_BOOT, 0);
}

static void set_deep_mode(void)
{
	if (deep_mode_init_done)
		__invoke_psci_fn_smc(SMCID_POWER_MANAGER, SMCSUBID_PM_DEEPCTRL, deep_mode, 0);
}

static int clr_suspend_notify(struct notifier_block *nb,
				     unsigned long event, void *dummy)
{
	if (event == PM_SUSPEND_PREPARE) {
		set_resume_method(UNDEFINED_WAKEUP);
		set_deep_mode();
	} else if (event == PM_POST_SUSPEND) {
		clr_deep_mode();
	}

	return NOTIFY_OK;
}

static struct notifier_block clr_suspend_notifier = {
	.notifier_call = clr_suspend_notify,
};

unsigned int is_pm_s2idle_mode(void)
{
	if (pm_suspend_target_state == PM_SUSPEND_TO_IDLE)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL_GPL(is_pm_s2idle_mode);

/*Call it as suspend_reason because of historical reasons. */
/*Actually, we should call it wakeup_reason.               */
static ssize_t suspend_reason_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	unsigned int len;
	unsigned int val;

	suspend_reason = get_resume_reason();
	val = get_resume_value();
	len = sprintf(buf, "%d\nreg val:0x%x\n", suspend_reason, val);

	return len;
}

static ssize_t suspend_reason_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	int ret;

	ret = kstrtouint(buf, 0, &suspend_reason);

	if (ret)
		return -EINVAL;

	return count;
}

static CLASS_ATTR_RW(suspend_reason);

/*Call it as deep_suspend flag when it's val is 1 means last str is deep suspend */
ssize_t deep_suspend_show(const struct class *class,
			   const struct class_attribute *attr,
			    char *buf)
{
	return sprintf(buf, "%d\n", deep_suspend);
}

static CLASS_ATTR_RO(deep_suspend);

struct suspend_cmd_t {
	const char *name;
	int (*store)(void *data, const char *buf, size_t count);
	int (*show)(void *data, char *buf);
};

struct suspend_mode_cmd_t {
	const char *version;
	struct suspend_cmd_t cmds[];
};

static int suspend_ddr_mode_store(void *data, const char *buf, size_t count)
{
	int ret;

	ret = kstrtouint(buf, 0, &suspend_mode);
	if (ret)
		return ret;

	__invoke_psci_fn_smc(SMCID_POWER_MANAGER, SMCSUBID_PM_DDR_ASR, suspend_mode, 0);
	return 0;
}

static int suspend_ddr_mode_show(void *data, char *buf)
{
	(void)data;

	return scnprintf(buf, PAGE_SIZE, "DDR mode=%s\n", suspend_mode ? "ASR" : "SelfRefresh");
}

bool ee_support_powerdown(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SMCID_POWER_MANAGER, SMCSUBID_PM_GET_EE_STAT, 0, 0, 0, 0, 0, 0, &res);

	return res.a0 ? 0 : res.a1;
}
EXPORT_SYMBOL_GPL(ee_support_powerdown);

static int suspend_ctrl_store(void *data, const char *buf, size_t count)
{
	char cmd[16];
	u32 arg1;

	(void)data;

	if (!deep_mode_init_done)
		return 0;

	if (sscanf(buf, "%15s %x", cmd, &arg1) == 2) {
		deep_mode = SET_MODULE_VAL(VERSION, PM_VERSION, arg1);
	} else {
		return -EINVAL;
	}

	return 0;
}

static int suspend_ctrl_show(void *data, char *buf)
{
	(void)data;

	if (!deep_mode_init_done)
		return 0;

	return scnprintf(buf, PAGE_SIZE, "SUSPEND MODE=0x%x\n", deep_mode);
}

static inline int deep_mode_init(struct platform_device *pdev)
{
	if (!of_property_read_u32(pdev->dev.of_node, "deep_mode", &deep_mode)) {
		//__invoke_psci_fn_smc(SMCID_POWER_MANAGER, SMCSUBID_PM_DEEPCTRL, deep_mode, 0);
		deep_mode_init_done = 1;
	}

	clr_deep_mode();
	return 0;
}

static struct suspend_mode_cmd_t suspend_cmds = {
	.version = "V01",
	.cmds = {
		{"0", suspend_ddr_mode_store, suspend_ddr_mode_show},
		{"1", suspend_ddr_mode_store, NULL},
		{"DEEPCTRL", suspend_ctrl_store, suspend_ctrl_show},
		{NULL, NULL, NULL},
	}
};

static ssize_t suspend_mode_show(const struct class *class,
			    const struct class_attribute *attr,
			    char *buf)
{
	unsigned int len = 0;
	int i;

	if (!deep_mode_init_done) {
		return scnprintf(buf, PAGE_SIZE, "Don't support deep mode config.\n");
	}

	for (i = 0; suspend_cmds.cmds[i].name; i++) {
		if (suspend_cmds.cmds[i].show)
			len += suspend_cmds.cmds[i].show(&suspend_cmds, buf + len);
	}

	return len;
}

static ssize_t suspend_mode_store(const struct class *class,
			     const struct class_attribute *attr,
			     const char *buf, size_t count)
{
	char cmd[32];
	int ret, i;

	if (sscanf(buf, "%31s", cmd) != 1)
		return -EINVAL;

	for (i = 0; suspend_cmds.cmds[i].name; i++) {
		if (strcmp(cmd, suspend_cmds.cmds[i].name) == 0) {
			ret = suspend_cmds.cmds[i].store(&suspend_cmds, buf, count);
			if (ret)
				return ret;
			return count;
		}
	}

	pr_warn("unknown command:%s\n", cmd);
	return -EINVAL;
}

static int panic_notify(struct notifier_block *self,
			unsigned long cmd, void *ptr)
{
	clr_deep_mode();

	return NOTIFY_DONE;
}

static CLASS_ATTR_RW(suspend_mode);
static struct notifier_block panic_notifier = {
	.notifier_call	= panic_notify,
};

static ssize_t time_out_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	unsigned int val = 0, len;

	val = readl_relaxed(debug_reg);
	len = sprintf(buf, "%d\n", val);

	return len;
}

static int sys_time_out;
static ssize_t time_out_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	unsigned int time_out;
	int ret;

	ret = kstrtouint(buf, 10, &time_out);
	switch (ret) {
	case 0:
		sys_time_out = time_out;
		writel_relaxed(time_out, debug_reg);
		break;
	default:
		return -EINVAL;
	}

	return count;
}

static CLASS_ATTR_RW(time_out);

/* The sys file interface: read state*/
static ssize_t power_event_state_show(const struct class *class,
								const struct class_attribute *attr,
								char *buf)
{
	char def[] = "NULL";
	char msg[] = "POWER_PRESSED";
	unsigned int len = -1;

	if (power_event_available == 1) {
		len = snprintf(buf, sizeof(msg) + 1, "%s\n", msg);
		return len;
	}

	len = snprintf(buf, sizeof(def) + 1, "%s\n", def);

	return len;
}

static ssize_t power_event_state_store(const struct class *class,
								const struct class_attribute *attr,
								const char *buf, size_t count)
{
	char cmd[16];

	memset(cmd, 0, sizeof(cmd));
	size_t copy_len = min(count, sizeof(cmd) - 1);

	if (!buf) {
		pr_err("input buf is null.\n");
		return -EFAULT;
	}

	memcpy(cmd, buf, copy_len);
	cmd[copy_len] = '\0';

	// check input command: "clear" or "reset"
	if (strncmp(cmd, "clear", 5) == 0 || strncmp(cmd, "reset", 5) == 0) {
		power_event_available = 0;
		pr_info("Event flag cleared by user write\n");
		return copy_len;
	}

	return -EFAULT;
}

static CLASS_ATTR_RW(power_event_state);

/* The sys file interface: read name*/
static ssize_t power_event_name_show(const struct class *class,
								const struct class_attribute *attr,
								char *buf)
{
	char def[] = "NULL";
	unsigned int len = -1;

	if (power_event_available == 1) {
		len = snprintf(buf, sizeof(dev_name_event) + 1, "%s\n", dev_name_event);
		return len;
	}

	len = snprintf(buf, sizeof(def) + 1, "%s\n", def);

	return len;
}

static ssize_t power_event_name_store(const struct class *class,
								const struct class_attribute *attr,
								const char *buf, size_t count)
{
	return -EFAULT;
}

static CLASS_ATTR_RW(power_event_name);

static struct attribute *meson_pm_attrs[] = {
	&class_attr_suspend_reason.attr,
	&class_attr_suspend_mode.attr,
	&class_attr_time_out.attr,
	&class_attr_deep_suspend.attr,
#if IS_ENABLED(CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND)
	&class_attr_early_suspend_trigger.attr,
#endif
	&class_attr_suspend_debug.attr,
	&class_attr_power_event_state.attr,
	&class_attr_power_event_name.attr,
	NULL,
};

ATTRIBUTE_GROUPS(meson_pm);

static struct class meson_pm_class = {
	.name		= "meson_pm",
	.class_groups = meson_pm_groups,
};

/* power down action */
static void do_custom_power_action(void)
{
	power_event_available = 1;
}

/* Input Handler: do event */
static void power_handler_event(struct input_handle *handle,
								unsigned int type,
								unsigned int code, int value)
{
	size_t copy_len;

	if (type == EV_KEY && code == KEY_POWER && value == 1) {
		if (handle && handle->dev && handle->dev->name) {
			memset(dev_name_event, 0, sizeof(dev_name_event));
			copy_len = min(strlen(handle->dev->name), sizeof(dev_name_event) - 1);
			memcpy(dev_name_event, handle->dev->name, copy_len);
			dev_name_event[copy_len] = '\0';
		}
		pr_info("Power key pressed from device: (%s)\n", dev_name_event);
		do_custom_power_action();
	}
}

/* connect function: when handler match devices */
static int power_handler_connect(struct input_handler *handler,
								struct input_dev *dev,
								const struct input_device_id *id)
{
	struct input_handle *handle;
	int ret;

	handle = kzalloc(sizeof(*handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = DRIVER_NAME;

	ret = input_open_device(handle);
	if (ret) {
		pr_err("%s: Failed to open device %s\n", DRIVER_NAME, dev->name);
		kfree(handle);
		return ret;
	}

	ret = input_register_handle(handle);
	if (ret) {
		input_close_device(handle);
		kfree(handle);
		return ret;
	}

	pr_info("%s: Connected to device: %s\n", DRIVER_NAME, dev->name);
	return 0;
}

/* disconnect devices */
static void power_handler_disconnect(struct input_handle *handle)
{
	pr_info("%s: Disconnected from device: %s\n", DRIVER_NAME, handle->dev->name);
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

/* match all input devices */
static const struct input_device_id power_handler_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_KEYBIT,
		.keybit = { [BIT_WORD(KEY_POWER)] = BIT_MASK(KEY_POWER) },
		.driver_info = 1 },
	{ },
};
MODULE_DEVICE_TABLE(input, power_handler_ids);

/* Input Handler */
static struct input_handler power_input_handler = {
	.event      = power_handler_event,
	.connect    = power_handler_connect,
	.disconnect = power_handler_disconnect,
	.name       = DRIVER_NAME,
	.id_table   = power_handler_ids,
};

static int gx_pm_syscore_suspend(void)
{
	if (sys_time_out)
		writel_relaxed(sys_time_out, debug_reg);

	return 0;
}

/*clear wakeup reason*/
static void gx_pm_syscore_shutdown(void)
{
	u32 val;

	if (exit_reg && is_extd_resume_reason) {
		val = readl_relaxed(exit_reg);
		if ((val & 0x7f) == REMOTE_CUS_WAKEUP) {
			/*Defalut clear netflix wakeup reason*/
			val &= ~REMOTE_CUS_WAKEUP;
		} else if (clear_suspend_reason) {
			if ((val & 0x7f) == clear_suspend_reason)
				val &= ~clear_suspend_reason;
		}
		writel_relaxed(val, exit_reg);
	}

}

static void gx_pm_syscore_resume(void)
{
	sys_time_out = 0;
	set_resume_method(get_resume_reason());
}

static struct syscore_ops gx_pm_syscore_ops = {
	.suspend = gx_pm_syscore_suspend,
	.resume	= gx_pm_syscore_resume,
	.shutdown = gx_pm_syscore_shutdown,
};

static int __init gx_pm_init_ops(void)
{
	register_syscore_ops(&gx_pm_syscore_ops);
	return 0;
}

static int meson_pm_probe(struct platform_device *pdev)
{
	unsigned int irq_pwrctrl;
	int err;

	power_event_available = 0;

	if (!of_property_read_u32(pdev->dev.of_node,
				  "irq_pwrctrl", &irq_pwrctrl)) {
		pwr_ctrl_irq_set(irq_pwrctrl, 1, 0);
	}

	if (of_property_read_bool(pdev->dev.of_node, "extend_resume_reason"))
		is_extd_resume_reason = true;
	else
		is_extd_resume_reason = false;

	if (of_property_read_u32(pdev->dev.of_node, "clear_suspend_reason",
		&clear_suspend_reason)) {
		dev_info(&pdev->dev, "clear_suspend_reason not found, using default\n");
		clear_suspend_reason = 0;
	} else {
		dev_info(&pdev->dev, "clear_suspend_reason found: 0x%x\n", clear_suspend_reason);
	}

	deep_mode_init(pdev);

	debug_reg = of_iomap(pdev->dev.of_node, 0);
	if (!debug_reg)
		return -ENOMEM;
	exit_reg = of_iomap(pdev->dev.of_node, 1);
	if (!exit_reg)
		return -ENOMEM;

	set_resume_method(get_resume_reason());

	err = class_register(&meson_pm_class);
	if (unlikely(err))
		return err;

	// register input handler
	err = input_register_handler(&power_input_handler);
	if (err) {
		pr_err("%s: Failed to register input handler\n", DRIVER_NAME);
		return err;
	}

	gx_pm_init_ops();

#if IS_ENABLED(CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND)
	err = register_pm_notifier(&lgcy_early_suspend_notifier);
	if (unlikely(err))
		return err;
#endif
	if (of_property_read_bool(pdev->dev.of_node, "clr_resume_reason"))
		is_clr_resume_reason = true;
	else
		is_clr_resume_reason = false;

	err = register_pm_notifier(&clr_suspend_notifier);
	if (unlikely(err))
		return err;

	err = register_die_notifier(&panic_notifier);
	if (err != 0)
		return err;

	/* Register a call for panic conditions. */
	err = atomic_notifier_chain_register(&panic_notifier_list,
					     &panic_notifier);
	if (err != 0)
		return err;

	if (suspend_debug_flag)
		set_suspend_debug_flag(suspend_debug_flag);

	return 0;
}

static __maybe_unused void meson_pm_remove(struct platform_device *pdev)
{
	if (debug_reg)
		iounmap(debug_reg);
	if (exit_reg)
		iounmap(exit_reg);

	class_unregister(&meson_pm_class);
	input_unregister_handler(&power_input_handler);

#if IS_ENABLED(CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND)
	lgcy_early_suspend_exit(pdev);
#endif
}

static const struct of_device_id amlogic_pm_dt_match[] = {
	{.compatible = "amlogic, pm",
	},
	{}
};

static void meson_pm_shutdown(struct platform_device *pdev)
{
	u32 val;

	if (exit_reg && is_clr_resume_reason &&
			is_extd_resume_reason) {
		val = readl_relaxed(exit_reg);
		val &= (~0x7f);
		writel_relaxed(val, exit_reg);
	}
}

static int pm_platform_resume(struct device *dev)
{
	return 0;
}

static int pm_platform_suspend(struct device *dev)
{
	deep_suspend = 1;
	if (early_suspend_debug)
		pr_info("%s: set deep_suspend flag as 1\n", __func__);
	return 0;
}

static const struct dev_pm_ops pm_ops = {
	.resume = pm_platform_resume,
	.suspend = pm_platform_suspend,
};

static struct platform_driver meson_pm_driver = {
	.probe = meson_pm_probe,
	.driver = {
		   .name = "pm-meson",
		   .owner = THIS_MODULE,
		   .pm = &pm_ops,
		   .of_match_table = amlogic_pm_dt_match,
		   },
	.remove = __exit_p(meson_pm_remove),
	.shutdown = meson_pm_shutdown,
};

int __init pm_init(void)
{
	return platform_driver_register(&meson_pm_driver);
}

void __exit pm_exit(void)
{
	platform_driver_unregister(&meson_pm_driver);
}
