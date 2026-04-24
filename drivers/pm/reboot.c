// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/reboot.h>

#include <asm/system_misc.h>

#include <linux/amlogic/iomap.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/reboot.h>
//#include <asm/compiler.h>
#include <linux/kdebug.h>
#include <linux/amlogic/arm-smccc.h>
#include <linux/panic_notifier.h>
#include <linux/syscore_ops.h>
#include <linux/cpu.h>
#include <linux/suspend.h>
#include <uapi/amlogic/aml_reboot.h>
#include <linux/miscdevice.h>
#include <linux/suspend.h>

#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG)
#include <linux/amlogic/gki_module.h>
#endif

static void __iomem *reboot_reason_vaddr;
static u32 psci_function_id_restart;
static u32 psci_function_id_poweroff;
static char *kernel_panic;
static char *reboot_check;
static int hib_enable;
/* Represents if it can support reboot reason extension - */
/* which can support up to 128 kind of reboot reasons */
static bool support_reboot_reason_ext;

static struct reboot_reason_str reboot_reason_name[MESON_MAX_REBOOT] = {
	[MESON_COLD_REBOOT] = { .name = "cold reboot" },
	[MESON_NORMAL_BOOT] = { .name = "normal boot" },
	[MESON_FACTORY_RESET_REBOOT] = { .name = "factory reset reboot" },
	[MESON_UPDATE_REBOOT] = { .name = "update reboot" },
	[MESON_FASTBOOT_REBOOT] = { .name = "fastboot reboot" },
	[MESON_UBOOT_SUSPEND] = { .name = "uboot suspend" },
	[MESON_HIBERNATE] = { .name = "hibernate" },
	[MESON_BOOTLOADER_REBOOT] = { .name = "bootloader reboot" },
	[MESON_RPMBP_REBOOT] = { .name = "rpmbp reboot" },
	[MESON_QUIESCENT_REBOOT] = { .name = "quiescent reboot" },
	[MESON_RESCUEPARTY_REBOOT] = { .name = "rescueparty reboot" },
	[MESON_KERNEL_PANIC] = { .name = "kernel panic" },
	[MESON_RECOVERY_QUIESCENT_REBOOT] = {
		.name = "recovery quiescent reboot" },
	[MESON_FFV_REBOOT] = { .name = "ffv reboot" },
	[MESON_FACTORY_MODE_REBOOT] = { .name = "factory mode reboot" },
	[MESON_BIST_MODE_REBOOT] = { .name = "bist mode reboot" },
	[MESON_AOCPU_WATCHDOG_REBOOT] = { .name = "aocpu watchdog reboot" },
	[MESON_RTC_QUIESCENT_REBOOT] = { .name = "rtc quiescent reboot" },
};

#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG)
static unsigned int scramble_reg;
void __iomem *scramble_vaddr;
__module_param(scramble_reg, uint, 0644);

static int scramble_reg_setup(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (kstrtouint(buf, 16, &scramble_reg)) {
		pr_err("!scramble_reg error: %s, scramble_reg=%x\n", buf, scramble_reg);
		return -EINVAL;
	}

	pr_info("scramble_reg=%x\n", scramble_reg);
	return 1;
}
__setup("scramble_reg=", scramble_reg_setup);

/*
 * scramble_clear_preserve() will clear scramble_reg bit0,
 * this will cause fresh ddr data after reboot
 */
static void scramble_clear_preserve(void)
{
	unsigned int val;

	if (scramble_vaddr) {
		val = readl(scramble_vaddr);
		val = val & (~0x1);
		writel(val, scramble_vaddr);
		pr_info("STARTUP_KEY_PRESERVE\n");
	}
}
#endif

static u32 get_reboot_reason_val(void)
{
	u32 value;
	u32 reboot_reason_val;

	if (!reboot_reason_vaddr) {
		pr_err("%s: reboot_reason_vaddr is NULL!\n", __func__);
		return 0;
	}

	value = readl(reboot_reason_vaddr);
	if (support_reboot_reason_ext)
		reboot_reason_val = (value >> 12) & 0xff;
	else
		reboot_reason_val = (value >> 12) & 0xf;

	return reboot_reason_val;
}

u32 get_reboot_reason(void)
{
	u32 reboot_reason;
	u32 reboot_reason_val;

	reboot_reason_val = get_reboot_reason_val();

	if (support_reboot_reason_ext)
		reboot_reason = reboot_reason_val & 0x7f;
	else
		reboot_reason = reboot_reason_val & 0xf;

	return reboot_reason;
}
EXPORT_SYMBOL_GPL(get_reboot_reason);

static u32 parse_reason(const char *cmd)
{
	u32 reboot_reason = MESON_NORMAL_BOOT;

	if (cmd) {
		if (strcmp(cmd, "recovery") == 0 ||
		    strcmp(cmd, "factory_reset") == 0)
			reboot_reason = MESON_FACTORY_RESET_REBOOT;
		else if (strcmp(cmd, "cold_boot") == 0)
			reboot_reason = MESON_COLD_REBOOT;
		else if (strcmp(cmd, "update") == 0)
			reboot_reason = MESON_UPDATE_REBOOT;
		else if (strcmp(cmd, "fastboot") == 0)
			reboot_reason = MESON_FASTBOOT_REBOOT;
		else if (strcmp(cmd, "bootloader") == 0 ||
			strcmp(cmd, "apexd-failed") == 0 ||
			strcmp(cmd, "boringssl-self-check-failed") == 0 ||
			strcmp(cmd, "bpfloader-failed") == 0 ||
			strcmp(cmd, "vold-failed") == 0 ||
			strcmp(cmd, "dm-verity device corrupted") == 0)
			reboot_reason = MESON_BOOTLOADER_REBOOT;
		else if (strcmp(cmd, "rpmbp") == 0)
			reboot_reason = MESON_RPMBP_REBOOT;
		else if (strcmp(cmd, "rescueparty") == 0)
			reboot_reason = MESON_RESCUEPARTY_REBOOT;
		else if (strcmp(cmd, "uboot_suspend") == 0)
			reboot_reason = MESON_UBOOT_SUSPEND;
		else if (strcmp(cmd, "quiescent") == 0 ||
			strcmp(cmd, "userrequested,recovery,quiescent") == 0 ||
			strcmp(cmd, "userrequested,quiescent") == 0 ||
			strcmp(cmd, "reboot-ab-update,quiescent") == 0 ||
			strcmp(cmd, "unattended,ota_update,quiescent") == 0 ||
			strcmp(cmd, ",quiescent") == 0)
			reboot_reason = MESON_QUIESCENT_REBOOT;
		else if (strcmp(cmd, "recovery,quiescent") == 0 ||
			 strcmp(cmd, "factory_reset,quiescent") == 0 ||
			 strcmp(cmd, "quiescent,recovery") == 0 ||
			 strcmp(cmd, "quiescent,factory_reset") == 0)
			reboot_reason = MESON_RECOVERY_QUIESCENT_REBOOT;
		else if (strcmp(cmd, "ffv_reboot") == 0)
			reboot_reason = MESON_FFV_REBOOT;
		else if (strcmp(cmd, "factory") == 0)
			reboot_reason = MESON_FACTORY_MODE_REBOOT;
		else if (strcmp(cmd, "Diagnostic") == 0)
			reboot_reason = MESON_BIST_MODE_REBOOT;
	} else {
		if (kernel_panic) {
			if (strcmp(kernel_panic, "kernel_panic") == 0)
				reboot_reason = MESON_KERNEL_PANIC;
		}
	}

	pr_info("reboot reason = %d, %s\n", reboot_reason,
			reboot_reason_name[reboot_reason].name);

	return reboot_reason;
}

static noinline int __invoke_psci_fn_smc(u64 function_id, u64 arg0, u64 arg1,
					 u64 arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc((unsigned long)function_id,
		      (unsigned long)arg0,
		      (unsigned long)arg1,
		      (unsigned long)arg2,
		      0, 0, 0, 0, &res);
	return res.a0;
}

void meson_smc_restart(u64 function_id, u64 reboot_reason)
{
	__invoke_psci_fn_smc(function_id,
			     reboot_reason, 0, 0);
}

void meson_common_restart(char mode, const char *cmd)
{
	u32 reboot_reason = parse_reason(cmd);

#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG)
	if (reboot_reason != MESON_KERNEL_PANIC)
		scramble_clear_preserve();
#endif

	if (psci_function_id_restart)
		meson_smc_restart((u64)psci_function_id_restart,
				  (u64)reboot_reason);
}

static int do_aml_restart(struct notifier_block *nb, unsigned long reboot_mode,
			  void *cmd)
{
	meson_common_restart(reboot_mode, cmd);

	return NOTIFY_DONE;
}

static void do_aml_poweroff(void)
{
#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG)
	scramble_clear_preserve();
#endif
	/* TODO: Add poweroff capability */
	__invoke_psci_fn_smc(0x82000042, 1, 0, 0);
	__invoke_psci_fn_smc(psci_function_id_poweroff,
			     0, 0, 0);
}

static int panic_notify(struct notifier_block *self,
			unsigned long cmd, void *ptr)
{
	kernel_panic = "kernel_panic";

	return NOTIFY_DONE;
}

static struct notifier_block panic_notifier = {
	.notifier_call	= panic_notify,
};

static int reboot_die_callback(struct notifier_block *self,
			unsigned long cmd, void *ptr)
{
	kernel_panic = "kernel_panic";

	return NOTIFY_DONE;
}

static struct notifier_block reboot_die_notifier = {
	.notifier_call	= reboot_die_callback,
};

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
ssize_t reboot_reason_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	unsigned int len;
	u32 reboot_reason;
	u32 reboot_reason_reg;

	reboot_reason_reg = get_reboot_reason_val();
	reboot_reason = get_reboot_reason();
	len = sprintf(buf, "reg val: 0x%x\nreboot reason = %d, %s\n",
			reboot_reason_reg, reboot_reason,
			reboot_reason_name[reboot_reason].name);

	return len;
}

static DEVICE_ATTR_RO(reboot_reason);
#endif

static ssize_t reset_test_store(struct device *dev,
			   struct device_attribute *attr,  const char *buf, size_t count)
{
	char s[64] = {0};
	u32 reason, i = 0;

	if (count > 64) {
		pr_err("%s: Error: string length %u is over maximum limits 64\n",
		       __func__, (u32)count);
		return -EINVAL;
	}

	while (*buf != '\n')
		s[i++] = *buf++;
	reason = parse_reason(s);

	pr_info("[%s %d]%s reset, reboot reason:%u\n", __func__, __LINE__, s, reason);
	do_aml_restart(NULL, 0, (void *)s);

	return count;
}

static DEVICE_ATTR_WO(reset_test);

static int reboot_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int reboot_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static long reboot_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct arm_smccc_res smc_res;
	int val, ret = 0;

	switch (cmd) {
	case REBOOT_GET:
		ret = get_reboot_reason();
		if (copy_to_user((int __user *)arg, &ret, sizeof(int)))
			return -EFAULT;
		break;
	case REBOOT_SET:
		if (copy_from_user(&val, (int __user *)arg, sizeof(int)))
			return -EFAULT;
		if (support_reboot_reason_ext)
			arm_smccc_smc(0x82000048, 1, val, 0, 0, 0, 0, 0, &smc_res);
		break;
	default:
		return -ENOTTY;
	}

	return 0;
}

#ifdef CONFIG_COMPAT
static long reboot_compat_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	long ret = 0;

	ret = reboot_ioctl(file, cmd, (ulong)compat_ptr(arg));
	return ret;
}
#endif

const struct file_operations reboot_fops = {
	.owner			= THIS_MODULE,
	.open			= reboot_open,
	.release		= reboot_release,
	.unlocked_ioctl = reboot_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= reboot_compat_ioctl,
#endif
};

static struct miscdevice reboot_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "reboot",
	.fops = &reboot_fops,
};

static void disable_non_bootcpu_shutdown(void)
{
	int error = 0, cpu, primary = 0;

	if (reboot_check)
		return;

	if (!cpu_online(primary))
		primary = cpumask_first(cpu_online_mask);

	if (hib_enable)
		cpu_hotplug_enable();

	cpu_hotplug_enable();

	for_each_online_cpu(cpu) {
		if (cpu == primary)
			continue;

		error = cpu_subsys.offline(get_cpu_device(cpu));
		if (error) {
			pr_err("Error taking CPU%d down: %d\n", cpu, error);
			break;
		}
	}

	if (!error)
		WARN_ON(num_online_cpus() > 1);
	else
		pr_err("Non-boot CPUs are not disabled\n");

	cpu_hotplug_disable();
}

#ifdef CONFIG_PM
static int reboot_pm_cb(struct notifier_block *notifier,
			      unsigned long pm_event,
			      void *unused)
{
	pr_info("%s called. pm_event:%lu.\n", __func__, pm_event);

	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
		hib_enable = 1;
		break;
	case PM_POST_HIBERNATION:
		hib_enable = 0;
		break;
	case PM_SUSPEND_PREPARE:
		break;
	case PM_POST_SUSPEND:
	case PM_RESTORE_PREPARE:
	case PM_POST_RESTORE:
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block reboot_pm_nb = {
	.notifier_call = reboot_pm_cb,
};

static int reboot_pm_notifier_register(void)
{
	return register_pm_notifier(&reboot_pm_nb);
}

static int reboot_pm_notifier_unregister(void)
{
	return unregister_pm_notifier(&reboot_pm_nb);
}
#else
static int reboot_pm_notifier_register(void)
{
	return 0;
}

static int reboot_pm_notifier_unregister(void)
{
	return 0;
}
#endif

static int do_aml_set_reboot_flag(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	switch (event) {
	case SYS_HALT:
		break;
	case SYS_POWER_OFF:
		break;
	case SYS_RESTART:
		reboot_check = "reboot_system";
		break;
	}

	return NOTIFY_DONE;
}

static struct syscore_ops disable_non_bootcpu_syscore_ops = {
	.shutdown		= disable_non_bootcpu_shutdown,
};

static int __init reboot_pm_init_ops(void)
{
	register_syscore_ops(&disable_non_bootcpu_syscore_ops);
	return 0;
}

static struct notifier_block aml_restart_nb = {
	.notifier_call = do_aml_restart,
	.priority = 130,
};

static struct notifier_block aml_set_reboot_flag = {
	.notifier_call = do_aml_set_reboot_flag,
};

static int aml_restart_probe(struct platform_device *pdev)
{
	u32 id;
	int ret;

	if (!of_property_read_u32(pdev->dev.of_node, "sys_reset", &id)) {
		psci_function_id_restart = id;
		register_restart_handler(&aml_restart_nb);
		ret = device_create_file(&pdev->dev, &dev_attr_reset_test);
		if (ret != 0) {
			ret = -1;
			goto error;
		}
	}

	if (!of_property_read_u32(pdev->dev.of_node, "sys_poweroff", &id)) {
		psci_function_id_poweroff = id;
		pm_power_off = do_aml_poweroff;
	}

	reboot_reason_vaddr = devm_platform_ioremap_resource(pdev, 0);
	if (!IS_ERR(reboot_reason_vaddr)) {
	#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		ret = device_create_file(&pdev->dev, &dev_attr_reboot_reason);
		if (ret != 0) {
			ret = -2;
			goto error;
		}
	#endif

		support_reboot_reason_ext = of_property_read_bool(pdev->dev.of_node,
				"extend_reboot_reason");
	}

	if (of_property_read_bool(pdev->dev.of_node,
					"dis_nb_cpus_in_shutdown")) {
		pr_debug("Enable disable_nonboot_cpus in syscore shutdown.\n");
		reboot_pm_init_ops();
	}

	/* Register a reboot_notifier*/
	ret = register_reboot_notifier(&aml_set_reboot_flag);
	if (ret != 0) {
		ret = -3;
		goto error;
	}

	ret = register_die_notifier(&reboot_die_notifier);
	if (ret != 0) {
		ret = -4;
		goto error;
	}

	/* Register a call for panic conditions. */
	ret = atomic_notifier_chain_register(&panic_notifier_list,
					     &panic_notifier);
	if (ret != 0) {
		ret = -5;
		goto error;
	}

	/* Register misc device*/
	ret = misc_register(&reboot_miscdev);
	if (ret) {
		ret = -6;
		goto error;
	}

#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG)
	if (scramble_reg)
		scramble_vaddr = ioremap(scramble_reg, 4);
#endif
	return 0;
error:
	pr_err("reboot driver init Failed=%d!\n", ret);
	return ret;
}

static const struct of_device_id of_aml_restart_match[] = {
	{ .compatible = "aml, reboot", },
	{},
};
MODULE_DEVICE_TABLE(of, of_aml_restart_match);

static struct platform_driver aml_restart_driver = {
	.probe = aml_restart_probe,
	.driver = {
		.name = "aml-restart",
		.of_match_table = of_match_ptr(of_aml_restart_match),
	},
};

int __init reboot_init(void)
{
	reboot_pm_notifier_register();
	return platform_driver_register(&aml_restart_driver);
}

void __exit reboot_exit(void)
{
	reboot_pm_notifier_unregister();
	platform_driver_unregister(&aml_restart_driver);
	misc_deregister(&reboot_miscdev);
}
