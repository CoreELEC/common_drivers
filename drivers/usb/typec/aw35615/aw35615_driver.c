// SPDX-License-Identifier: GPL-2.0
/*******************************************************************************
 * Copyright (c) 2020-2024 Shanghai Awinic Technology Co., Ltd. All Rights Reserved
 *******************************************************************************
 * Author        : awinic
 * Date          : 2021-09-09
 * Description   : .C file function description
 * Version       : 1.0
 * Function List :
 ******************************************************************************/
/* Standard Linux includes */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/of_device.h>
#include <linux/usb/typec.h>
#include <linux/extcon-provider.h>
#include <linux/interrupt.h>

/* Driver-specific includes */
#include "aw35615_global.h"
#include "platform_helpers.h"
#include "core.h"
#include "TypeC.h"

#ifdef AW_DEBUG
#include "dfs.h"
#endif // AW_DEBUG

#include "aw35615_driver.h"

#define AW35615_DRIVER_VERSION		"V1.7.3"

/******************************************************************************
 * Driver functions
 ******************************************************************************/

static const unsigned int aw35615_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_CHG_USB_SDP,
	EXTCON_CHG_USB_CDP,
	EXTCON_CHG_USB_DCP,
	EXTCON_CHG_USB_SLOW,
	EXTCON_CHG_USB_FAST,
	EXTCON_DISP_DP,
	EXTCON_NONE,
};

static const struct of_device_id aw35615_dt_match[] = {
	{ .compatible = AW35615_I2C_DEVICETREE_NAME },
	{},
};

static const struct i2c_device_id aw35615_i2c_device_id[] = {
	{ AW35615_I2C_DRIVER_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, aw35615_i2c_device_id);

static void aw35615_init_delay_work(struct work_struct *work)
{
	struct aw35615_chip *chip =
			container_of(work, struct aw35615_chip, init_delay_work.work);
	int ret;

	/* Logically dead code (DEADCODE) fix. */
#ifndef MESON_AW
	if (!chip)
		return;
#endif

	/*
	 * Initialize the core and enable the state machine
	 * Interrupt must be enabled before starting initialization
	 */
	aw_InitializeCore(chip);

	/* Enable interrupts after successful core/GPIO initialization */
	ret = aw_EnableInterrupts(chip);
	if (ret) {
		AW_LOG("Unable to enable interrupts! Error code: %d\n", ret);
		return;
	}

	if (platform_get_device_irq_state(chip->port.PortID)) {
		/* Plug-in typec power-on interrupt detection */
		if (!chip->queued) {
			chip->queued = AW_TRUE;
			AW_LOG("Plug-in typec power-on interrupt detection\n");
			queue_work(chip->highpri_wq, &chip->sm_worker);
		}
	}

	AW_LOG("Core is initialized!\n");
}

static void aw35615_bist_work(struct work_struct *work)
{
	struct aw35615_chip *chip =
			container_of(work, struct aw35615_chip, bist_work);
	AW_U8 data_buf[3];

	if (!chip) {
		pr_err("AWINIC  %s - Chip structure is NULL!\n", __func__);
		return;
	}

	if (chip->port.PolicyIsSource == 0) {
		DeviceRead(&chip->port, regInterruptb, 2, &data_buf[0]);
		DeviceRead(&chip->port, regInterrupt, 1, &data_buf[2]);
		AW_LOG("regInterruptb = 0x%x regStatus0 = 0x%x regInterrupt = 0x%x\n", data_buf[0], data_buf[1], data_buf[2]);
		if (data_buf[0] & 0x1) {
			hrtimer_start(&chip->bist_timer, ktime_set(chip->source_end_timer / 1000, chip->source_end_timer * 1000000), HRTIMER_MODE_REL);
			AW_LOG("go sink check\n");
		} else {
			AW_LOG("go sink set SDAC\n");
			chip->port.Registers.Slice.byte = chip->sink_reg_bist;
			DeviceWrite(&chip->port, regSlice, 1, &chip->port.Registers.Slice.byte);
		}
	}

	if (chip->port.PolicyIsSource == 1) {
		chip->port.SOURCE_Flag_end = 1;
		DeviceRead(&chip->port, regInterruptb, 2, &data_buf[0]);
		DeviceRead(&chip->port, regInterrupt, 1, &data_buf[2]);
		AW_LOG("regInterruptb = 0x%x regStatus0 = 0x%x regInterrupt = 0x%x\n", data_buf[0], data_buf[1], data_buf[2]);
		if (data_buf[0] & 0x1) {
			hrtimer_start(&chip->bist_timer, ktime_set(chip->source_end_timer / 1000, chip->source_end_timer * 1000000), HRTIMER_MODE_REL);
			AW_LOG("go source check\n");
		} else {
			AW_LOG("go source set SDAC\n");
			chip->port.Registers.Slice.byte = chip->source_reg_bist;
			DeviceWrite(&chip->port, regSlice, 1, &chip->port.Registers.Slice.byte);
		}
	}
}

static enum hrtimer_restart aw35615_bist_timer_func(struct hrtimer *p_hrtimer)
{
	struct aw35615_chip *chip = container_of(p_hrtimer, struct aw35615_chip, bist_timer);

	schedule_work(&chip->bist_work);
	return HRTIMER_NORESTART;
}

#ifdef AW_HAVE_LPD
static void aw35615_lpd_check_work(struct work_struct *work)
{
	struct aw35615_chip *chip =
			container_of(work, struct aw35615_chip, lpd_check_work);

	if (!chip) {
		pr_err("AWINIC  %s - Chip structure is NULL!\n", __func__);
		return;
	}
	if ((chip->lpd_check_num == 0) || (chip->toggle_check_num == 0)) {
		AW_LOG("notify ldp\n");
		notify_observers(LPD_NOTICE, chip->port.PortID);
	}
	AW_LOG("lpd_check_num = %d toggle_check_num = %d\n", chip->lpd_check_num, chip->toggle_check_num);
	chip->lpd_check_num = chip->lpd_check_num_bak;
	chip->toggle_check_num = chip->lpd_check_num_bak;
	chip->lpd_check_enable = AW_TRUE;
}

static enum hrtimer_restart aw35615_lpd_timer_func(struct hrtimer *p_hrtimer)
{
	struct aw35615_chip *chip = container_of(p_hrtimer, struct aw35615_chip, lpd_timer);

	AW_LOG("enter\n");
	schedule_work(&chip->lpd_check_work);
	return HRTIMER_NORESTART;
}
#endif /* AW_HAVE_LPD */

static int init_extcon(struct aw35615_chip *chip)
{
	int ret = 0;

	chip->extcon = devm_extcon_dev_allocate(chip->dev, aw35615_cable);
	if (IS_ERR(chip->extcon)) {
		dev_err(chip->dev, "allocat extcon failed\n");
		return PTR_ERR(chip->extcon);
	}

	ret = devm_extcon_dev_register(chip->dev, chip->extcon);
	if (ret) {
		dev_err(chip->dev, "failed to register extcon: %d\n",
			ret);
		return ret;
	}

	ret = extcon_set_property_capability(chip->extcon, EXTCON_USB,
						 EXTCON_PROP_USB_TYPEC_POLARITY);
	if (ret) {
		dev_err(chip->dev,
			"failed to set USB property capability: %d\n",
			ret);
		return ret;
	}

	ret = extcon_set_property_capability(chip->extcon, EXTCON_USB_HOST,
						 EXTCON_PROP_USB_TYPEC_POLARITY);
	if (ret) {
		dev_err(chip->dev,
			"failed to set USB_HOST property capability: %d\n",
			ret);
		return ret;
	}

	ret = extcon_set_property_capability(chip->extcon, EXTCON_DISP_DP,
						 EXTCON_PROP_USB_TYPEC_POLARITY);
	if (ret) {
		dev_err(chip->dev,
			"failed to set DISP_DP property capability: %d\n",
			ret);
		return ret;
	}

	ret = extcon_set_property_capability(chip->extcon, EXTCON_USB,
						 EXTCON_PROP_USB_SS);
	if (ret) {
		dev_err(chip->dev,
			"failed to set USB USB_SS property capability: %d\n",
			ret);
		return ret;
	}

	ret = extcon_set_property_capability(chip->extcon, EXTCON_USB_HOST,
						 EXTCON_PROP_USB_SS);
	if (ret) {
		dev_err(chip->dev,
			"failed to set USB_HOST USB_SS property capability: %d\n",
			ret);
		return ret;
	}

	ret = extcon_set_property_capability(chip->extcon, EXTCON_DISP_DP,
						 EXTCON_PROP_USB_SS);
	if (ret) {
		dev_err(chip->dev,
			"failed to set DISP_DP USB_SS property capability: %d\n",
			ret);
		return ret;
	}

	ret = extcon_set_property_capability(chip->extcon, EXTCON_CHG_USB_FAST,
						 EXTCON_PROP_USB_TYPEC_POLARITY);
	if (ret) {
		dev_err(chip->dev,
			"failed to set USB_PD property capability: %d\n", ret);
		return ret;
	}

	//extcon_set_state_sync(chip->extcon, EXTCON_USB, true);
	return ret;
}

static int aw35615_typec_dr_set(struct typec_port *port,
                                enum typec_data_role role)
{
	return 0;
}

static int aw35615_typec_pr_set(struct typec_port *port,
                                enum typec_role role)
{
	return 0;
}

static int aw35615_typec_port_type_set(struct typec_port *port,
                                enum typec_port_type type)
{
	return 0;
}

const struct typec_operations typec_ops = {
	.dr_set = aw35615_typec_dr_set,
	.pr_set = aw35615_typec_pr_set,
	.port_type_set = aw35615_typec_port_type_set,
};

#ifdef AW_KERNEL_VER_OVER_6_6_0
static int aw35615_probe(struct i2c_client *client)
#else
static int aw35615_probe(struct i2c_client *client, const struct i2c_device_id *id)
#endif
{
	struct aw35615_chip *chip;
	struct i2c_adapter *adapter = client->adapter;
	int ret = 0;

	AW_LOG("enter\n");

	/* Make sure probe was called on a compatible device */
	if (!of_match_device(aw35615_dt_match, &client->dev)) {
		dev_err(&client->dev,
				"AWINIC  %s - Error: Device tree mismatch!\n", __func__);
		return -EINVAL;
	}

	/* Allocate space for our chip structure (devm_* is managed by the device) */
	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		/* dev_err(&client->dev, "%s allocate memory\n", __func__); */
		return -ENOMEM;
	}

	chip->client = client;
	chip->dev = &client->dev;
	/* Assign our struct as the client's driverdata */
	i2c_set_clientdata(client, chip);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_I2C_BLOCK)) {
		dev_err(&client->dev,
			"I2C/SMBus block functionality not supported!\n");
		return -ENODEV;
	}

	/* Verify that our device exists and that it's what we expect */
	if (!aw_IsDeviceValid(chip)) {
		dev_err(&client->dev,
				"AWINIC  %s - Error: Unable to communicate with device!\n",
				__func__);
		devm_kfree(&client->dev, chip);
		chip = NULL;
		return -EIO;
	}

#ifdef AW_KERNEL_VER_OVER_5_7_0
	cpu_latency_qos_add_request(&chip->pm_gos_request, PM_QOS_DEFAULT_VALUE);
#else
	pm_qos_add_request(&chip->pm_gos_request,
			PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
#endif

	/* Initialize the chip's data members */
	aw_InitChipData(chip);

	/* Initialize the platform's GPIO pins and IRQ */
	ret = aw_parse_dts(chip);
	if (ret < 0) {
		dev_err(&client->dev,
				"AWINIC  %s - Error: Unable to initialize GPIO!\n", __func__);
		devm_kfree(&client->dev, chip);
		chip = NULL;
		return ret;
	}

	aw35615_SetChip(chip);

	if (chip->port.PortID == 0)
		aw35615_init_event_handler();

	/* Initialize semaphore*/
	sema_init(&chip->suspend_lock, 1);

	/* Initialize sysfs file accessors */
	aw_Sysfs_Init(chip);
	ret = init_extcon(chip);
	if (ret) {
		dev_err(&client->dev, "init_extcon failed!\n");
		return ret;
	}
#ifdef AW_DEBUG
	/* Initialize debugfs file accessors */
	aw_DFS_Init(chip);
	AW_LOG(" DebugFS nodes created!\n");
#endif // AW_DEBUG

#ifdef AW_HAVE_LPD
	INIT_WORK(&chip->lpd_check_work, aw35615_lpd_check_work);
	hrtimer_init(&chip->lpd_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	chip->lpd_timer.function = aw35615_lpd_timer_func;
#endif /* AW_HAVE_LPD */

	INIT_WORK(&chip->bist_work, aw35615_bist_work);
	hrtimer_init(&chip->bist_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	chip->bist_timer.function = aw35615_bist_timer_func;

	/* delay init */
	INIT_DELAYED_WORK(&chip->init_delay_work, aw35615_init_delay_work);
	schedule_delayed_work(&chip->init_delay_work, msecs_to_jiffies(3000));

	chip->typec_caps.type = TYPEC_PORT_DRP;
	chip->typec_caps.data = TYPEC_PORT_DRD;
	chip->typec_caps.revision = 0x0130;
	chip->typec_caps.pd_revision = 0x0300;
	chip->typec_caps.ops = &typec_ops;
	chip->typec_caps.driver_data = chip;
	chip->partner_desc.identity = &chip->partner_identity;

	chip->typec_port = typec_register_port(chip->dev, &chip->typec_caps);
	if (IS_ERR(chip->typec_port)) {
		AW_LOG("could not register typec port\n");
		return PTR_ERR(chip->typec_port);
	}

	AW_LOG(" AWINIC Driver loaded successfully!\n");

	return ret;
}

#ifdef AW_KERNEL_VER_OVER_6_1_0
static void aw35615_remove(struct i2c_client *client)
#else
static int aw35615_remove(struct i2c_client *client)
#endif
{
	struct aw35615_chip *chip = i2c_get_clientdata(client);

	AW_LOG(" Removing aw35615 device!\n");

#ifdef AW_DEBUG
	/* Remove debugfs file accessors */
	aw_DFS_Cleanup(chip);
	AW_LOG(" DebugFS nodes removed.\n");
#endif // AW_DEBUG

	aw_Sysfs_DeInit(chip);
	aw_GPIO_Cleanup(chip);
#ifdef AW_KERNEL_VER_OVER_5_7_0
	if (cpu_latency_qos_request_active(&chip->pm_gos_request))
		cpu_latency_qos_remove_request(&chip->pm_gos_request);
#else
	if (pm_qos_request_active(&chip->pm_gos_request))
		pm_qos_remove_request(&chip->pm_gos_request);
#endif

	AW_LOG(" AW35615 device removed from driver\n");
#ifndef AW_KERNEL_VER_OVER_6_1_0
	return 0;
#endif
}

static void aw35615_shutdown(struct i2c_client *client)
{
	AW_U8 reset = 0x01;
	AW_U8 data = 0x40;
	AW_U8 length = 0x01;
	AW_U8 reg_data;
#ifdef MESON_AW
	int ret = 0;
#else
	AW_BOOL ret = 0;
#endif
	struct aw35615_chip *chip = i2c_get_clientdata(client);

	if (!chip) {
		pr_err("AW35615 shutdown - Chip structure is NULL!\n");
		return;
	}

	core_enable_typec(&chip->port, AW_FALSE);
	if (chip->gpio_IntN_irq)
		disable_irq(chip->gpio_IntN_irq);
	cancel_work_sync(&chip->sm_worker);
	alarm_cancel(&chip->alarmtimer);
	aw_GPIO_Cleanup(chip);
	SetPEState(&chip->port, peDisabled);
	SetTypeCState(&chip->port, Unattached);

	ret = DeviceWrite(&chip->port, regControl3, length, &data);
	if (ret < 0)
		pr_err("send hardreset failed, ret = %d\n", ret);

	//SetStateUnattached(&chip->port);

	/* keep the cc open status 20ms */
	mdelay(5);
	ret = DeviceWrite(&chip->port, regReset, length, &reset);
	if (ret < 0)
		pr_err("device Reset failed, ret = %d\n", ret);

	reg_data = 0x00;
	DeviceWrite(&chip->port, regPower, 1, &reg_data);
	reg_data = 0x03;
	DeviceWrite(&chip->port, regSwitches0, 1, &reg_data);

	AW_LOG("aw35615 device shutdown!\n");
}

static int aw35615_i2c_resume(struct device *dev)
{
	struct aw35615_chip *chip;
	struct i2c_client *client = to_i2c_client(dev);

	if (client) {
		chip = i2c_get_clientdata(client);
		if (chip)
			up(&chip->suspend_lock);
	}

	return 0;
}

static int aw35615_i2c_suspend(struct device *dev)
{
	struct aw35615_chip *chip = NULL;
	struct i2c_client *client =  to_i2c_client(dev);
	int ret = 0;
	AW_U8 val[10];
	AW_U8 val1;

	if (client) {
		chip = i2c_get_clientdata(client);
		if (chip)
			down(&chip->suspend_lock);
#ifdef MESON_AW
		else
			return -ENOENT;
#endif
	}

	AW_LOG("port%d TC_ST=%d PE_ST=%d\n", chip->port.PortID, chip->port.ConnState, chip->port.PolicyState);
	ret = DeviceRead(&chip->port, regSwitches0, 10, val);
	if (ret < 0) {
		pr_err("error: read register fail\n");
		return 0;
	}
	AW_LOG("reg:[2:0x%02X 4:0x%02X 6:0x%02X 8:0x%02X a:0x%02X b:0x%02X ]\n",
			val[0], val[2], val[4], val[6], val[8], val[9]);

	ret = DeviceRead(&chip->port, regStatus0, 1, &val1);
	if (ret < 0) {
		pr_err("error: read register fail\n");
		return 0;
	}
	AW_LOG("reg:[40:0x%02X]\n", val1);

	return 0;
}

#ifdef CONFIG_PM
static const struct dev_pm_ops aw35615_dev_pm_ops = {
	.suspend = aw35615_i2c_suspend,
	.resume  = aw35615_i2c_resume,
};
#endif

static struct i2c_driver aw35615_driver = {
	.driver = {
		.name = AW35615_I2C_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(aw35615_dt_match),
#ifdef CONFIG_PM
		.pm = &aw35615_dev_pm_ops,
#endif
	},
	.probe = aw35615_probe,
	.remove = aw35615_remove,
	.shutdown = aw35615_shutdown,
	.id_table = aw35615_i2c_device_id,
};

static int __init aw35615_init(void)
{
	AW_LOG("Start driver init version %s\n", AW35615_DRIVER_VERSION);

	return i2c_add_driver(&aw35615_driver);
}

static void __exit aw35615_exit(void)
{
	i2c_del_driver(&aw35615_driver);
	AW_LOG(" Driver deleted\n");
}

/*******************************************************************************
 * Driver macros
 ******************************************************************************/
module_init(aw35615_init);
module_exit(aw35615_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AWINIC AW35615 Driver");
MODULE_AUTHOR("AWINIC");
