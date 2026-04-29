# SPDX-License-Identifier: (GPL-2.0+ OR MIT)

subdir-ccflags-y += "-D__module_param(name, type, perm)="
subdir-ccflags-y += "-D__module_param_named(name, value, type, perm)="
subdir-ccflags-y += "-D__module_param_array(name, type, nump, perm)="
subdir-ccflags-y += "-D__module_param_cb(name, ops, arg, perm)="
subdir-ccflags-y += "-D__module_param_array_named(name, array, type, nump, perm)="
subdir-ccflags-$(CONFIG_PREEMPT_RT) += $(call cc-disable-warning, unused-variable)

obj-$(CONFIG_AMLOGIC_GKI_TOOL)		+= gki_tool/
obj-y					+= debug/
obj-y					+= tty/
obj-$(CONFIG_AMLOGIC_HWSPINLOCK)	+= hwspinlock/
obj-$(CONFIG_AMLOGIC_SECMON)		+= secmon/
obj-y					+= memory_ext/
obj-$(CONFIG_AMLOGIC_MEMORY_DEBUG)	+= memory_debug/
obj-y					+= clk/
obj-$(CONFIG_AMLOGIC_GPIO)		+= gpio/
obj-$(CONFIG_MAILBOX)			+= mailbox/
obj-y					+= pwm/
obj-$(CONFIG_AMLOGIC_REGULATOR)		+= regulator/
obj-$(CONFIG_RESET_CONTROLLER)		+= reset/
obj-y					+= power/
obj-$(CONFIG_AMLOGIC_CPU_INFO)		+= cpu_info/
obj-$(CONFIG_CPU_FREQ)			+= cpufreq/
obj-$(CONFIG_THERMAL)			+= thermal/
obj-y					+= mmc/
obj-$(CONFIG_AMLOGIC_EFUSE_UNIFYKEY)	+= efuse_unifykey/
obj-y					+= i2c/
obj-$(CONFIG_SPI)			+= spi/
obj-y					+= char/
obj-$(CONFIG_AMLOGIC_TEE)		+= tee/
obj-$(CONFIG_AMLOGIC_AMLTEE)		+= aml_tee/
obj-$(CONFIG_AMLOGIC_PM)		+= pm/
obj-$(CONFIG_IIO)			+= iio/
obj-$(CONFIG_INPUT)			+= input/
obj-$(CONFIG_WATCHDOG)			+= watchdog/
obj-$(CONFIG_AMLOGIC_MEDIA_WRAPPER)	+= dhp/
obj-$(CONFIG_AMLOGIC_UVM)		+= uvm/
obj-y					+= dvb/
obj-y					+= dvb_ci/
obj-$(CONFIG_AMLOGIC_MEDIA_ENABLE)	+= media/
obj-$(CONFIG_AMLOGIC_WATERMARK)		+= aml_watermark/
obj-$(CONFIG_AMLOGIC_TRUSTED_KEYS)	+= aml_trusted_keys/
obj-$(CONFIG_AMLOGIC_AMFC)		+= amfc/
obj-$(CONFIG_AMLOGIC_USB)		+= usb/
obj-$(CONFIG_AMLOGIC_IRBLASTER)		+= irblaster/
obj-$(CONFIG_AMLOGIC_DRM)		+= drm/
obj-$(CONFIG_AMLOGIC_CRYPTO_DMA)	+= crypto/
obj-$(CONFIG_RTC_LIB)			+= rtc/
obj-y					+= pci/
obj-$(CONFIG_AMLOGIC_EXTERNAL_DRIVER)	+= external/
obj-$(CONFIG_AMLOGIC_SOC_INFO)		+= soc_info/
obj-$(CONFIG_AMLOGIC_HOST_DRIVER)	+= host/
obj-$(CONFIG_AMLOGIC_WIRELESS)		+= wireless/
obj-$(CONFIG_AMLOGIC_LED)		+= led/
obj-$(CONFIG_AMLOGIC_INPHY)		+= net/
obj-$(CONFIG_AMLOGIC_MTD_COMMON)	+= mtd/common/
obj-$(CONFIG_AMLOGIC_MTD_SPI_NAND)	+= mtd/spinand/
obj-$(CONFIG_AMLOGIC_MTD_NAND)		+= mtd/nand/
obj-$(CONFIG_AMLOGIC_MTD_SPI_NOR)	+= mtd/spinor/
obj-$(CONFIG_AMLOGIC_FREERTOS)		+= freertos/
obj-$(CONFIG_AMLOGIC_JTAG_MESON)	+= jtag/
obj-y					+= android/
obj-$(CONFIG_AMLOGIC_YAFFS_FS)		+= yaffs2/
obj-$(CONFIG_AMLOGIC_MFD_PMIC6B)	+= mfd/
obj-$(CONFIG_AMLOGIC_AUDIO_BRIDGE)	+= audiobridge/
obj-$(CONFIG_AMLOGIC_MEDIA_ALGORITHM)	+= algorithm/
obj-$(CONFIG_AMLOGIC_SOC_TIMESTAMP)	+= timestamp/
obj-$(CONFIG_AMLOGIC_SECKEY)		+= seckey/
obj-$(CONFIG_AMLOGIC_USBCAM)		+= dvb_usbci/
obj-$(CONFIG_AMLOGIC_ENV_ENCRYPT)	+= uboot_env/

KBUILD_CFLAGS_MODULE += $(GKI_EXT_KERNEL_MODULE_PREDEFINE)
all:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules $(GKI_EXT_KERNEL_MODULE_CONFIG)

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules_install

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) clean
