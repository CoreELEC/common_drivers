# SPDX-License-Identifier: GPL-2.0
load(":project/project.bzl", "project_configs")

AMLOGIC_COMMON_PRIVATE_HEADERS = [
    "kernel/sched/**/*.h",
]

OEM_PROJECT_MODULES = [

]

AMLOGIC_GKI20_MODULES = [
    "common_drivers/drivers/memory_ext/aml_cma.ko",
    "//common_drivers/drivers/memory_ext:aml_cma",
    "common_drivers/drivers/memory_ext/mem_debug.ko",
    "//common_drivers/drivers/memory_ext:mem_debug",
    "common_drivers/drivers/memory_ext/page_trace.ko",
    "//common_drivers/drivers/memory_ext:page_trace",
]

AMLOGIC_GKI10_MODULES = [

]

AMLOGIC_ANDROID_MODULES = [
    "common_drivers/drivers/tty/serial/amlogic-uart.ko",
    "//common_drivers/drivers/tty/serial:amlogic-uart",
]

AMLOGIC_COMMON_MODULES = [
    # keep sorted
    "arch/arm64/crypto/sha1-ce.ko",
    "drivers/dma-buf/heaps/system_heap.ko",
    "drivers/gpu/drm/drm_dma_helper.ko",
    "drivers/gpu/drm/display/drm_display_helper.ko",
    "drivers/gpu/drm/display/drm_dp_aux_bus.ko",
    "drivers/i2c/i2c-dev.ko",
    "drivers/leds/leds-gpio.ko",
    "drivers/net/mdio/mdio-mux.ko",
    "drivers/net/pcs/pcs_xpcs.ko",
    "drivers/regulator/gpio-regulator.ko",
    "drivers/clk/clk-scmi.ko",
    "net/mac80211/mac80211.ko",
    "net/wireless/cfg80211.ko",
    "common_drivers/drivers/amfc/amlogic-soc-amfc.ko",
    "//common_drivers/drivers/amfc:amlogic-soc-amfc",
    "common_drivers/drivers/amfc/aml_erofs/amlogic-soc-erofs.ko",
    "//common_drivers/drivers/amfc:amlogic-soc-erofs",
    "common_drivers/drivers/amfc/aml_zram/aml-zram.ko",
    "//common_drivers/drivers/amfc:aml-zram",
    "common_drivers/drivers/amfc/sym_helper/amlogic-soc-sym_helper.ko",
    "//common_drivers/drivers/amfc:amlogic-soc-sym_helper",
    "common_drivers/drivers/aml_tee/optee/optee.ko",
    "//common_drivers/drivers/aml_tee:optee",
    "common_drivers/drivers/aml_tee/tee.ko",
    "//common_drivers/drivers/aml_tee:tee",
    "common_drivers/drivers/aml_watermark/aml_watermark.ko",
    "//common_drivers/drivers/aml_watermark:aml_watermark",
    "common_drivers/drivers/char/hw_random/amlogic-rng.ko",
    "//common_drivers/drivers/char/hw_random:amlogic-rng",
    "common_drivers/drivers/clk/meson/amlogic-clk.ko",
    "//common_drivers/drivers/clk/meson:amlogic-clk",
    "common_drivers/drivers/clk/meson/amlogic-clk-soc-g12a.ko",
    "//common_drivers/drivers/clk/meson:amlogic-clk-soc-g12a",
    "common_drivers/drivers/clk/meson/amlogic-clk-soc-s4.ko",
    "//common_drivers/drivers/clk/meson:amlogic-clk-soc-s4",
    "common_drivers/drivers/clk/meson/amlogic-clk-soc-s5.ko",
    "//common_drivers/drivers/clk/meson:amlogic-clk-soc-s5",
    "common_drivers/drivers/clk/meson/amlogic-clk-soc-s6.ko",
    "//common_drivers/drivers/clk/meson:amlogic-clk-soc-s6",
    "common_drivers/drivers/clk/meson/amlogic-clk-soc-s7.ko",
    "//common_drivers/drivers/clk/meson:amlogic-clk-soc-s7",
    "common_drivers/drivers/clk/meson/amlogic-clk-soc-s7d.ko",
    "//common_drivers/drivers/clk/meson:amlogic-clk-soc-s7d",
    "common_drivers/drivers/clk/meson/amlogic-clk-soc-sc2.ko",
    "//common_drivers/drivers/clk/meson:amlogic-clk-soc-sc2",
    "common_drivers/drivers/clk/meson/amlogic-clk-soc-t3.ko",
    "//common_drivers/drivers/clk/meson:amlogic-clk-soc-t3",
    "common_drivers/drivers/clk/meson/amlogic-clk-soc-t3x.ko",
    "//common_drivers/drivers/clk/meson:amlogic-clk-soc-t3x",
    "common_drivers/drivers/clk/meson/amlogic-clk-soc-t5m.ko",
    "//common_drivers/drivers/clk/meson:amlogic-clk-soc-t5m",
    "common_drivers/drivers/clk/meson/amlogic-clk-soc-t5w.ko",
    "//common_drivers/drivers/clk/meson:amlogic-clk-soc-t5w",
    "common_drivers/drivers/clk/meson/amlogic-clk-soc-t6d.ko",
    "//common_drivers/drivers/clk/meson:amlogic-clk-soc-t6d",
    "common_drivers/drivers/clk/meson/amlogic-clk-soc-t6w.ko",
    "//common_drivers/drivers/clk/meson:amlogic-clk-soc-t6w",
    "common_drivers/drivers/clk/meson/amlogic-clk-soc-t6x.ko",
    "//common_drivers/drivers/clk/meson:amlogic-clk-soc-t6x",
    "common_drivers/drivers/clk/meson/amlogic-clk-soc-t7.ko",
    "//common_drivers/drivers/clk/meson:amlogic-clk-soc-t7",
    "common_drivers/drivers/cpufreq/amlogic-cpufreq.ko",
    "//common_drivers/drivers/cpufreq:amlogic-cpufreq",
    "common_drivers/drivers/cpu_info/amlogic-cpuinfo.ko",
    "//common_drivers/drivers/cpu_info:amlogic-cpuinfo",
    "common_drivers/drivers/crypto/amlogic-crypto-dma.ko",
    "//common_drivers/drivers/crypto:amlogic-crypto-dma",
    "common_drivers/drivers/debug/amlogic-audio-utils.ko",
    "//common_drivers/drivers/debug:amlogic-audio-utils",
    "common_drivers/drivers/debug/amlogic-debug.ko",
    "//common_drivers/drivers/debug:amlogic-debug",
    "common_drivers/drivers/debug/amlogic-debug-iotrace.ko",
    "//common_drivers/drivers/debug:amlogic-debug-iotrace",
    "common_drivers/drivers/drm/aml_drm.ko",
    "//common_drivers/drivers/drm:aml_drm",
	"common_drivers/drivers/uboot_env/amlogic-env.ko",
	"//common_drivers/drivers/uboot_env:amlogic-env",
    "common_drivers/drivers/uvm/aml_uvm.ko",
    "//common_drivers/drivers/uvm:aml_uvm",
    "common_drivers/drivers/dvb/amlogic-dvb.ko",
    "//common_drivers/drivers/dvb:amlogic-dvb",
    "common_drivers/drivers/dvb_ci/amlogic-dvb-ci.ko",
    "//common_drivers/drivers/dvb_ci:amlogic-dvb-ci",
    "common_drivers/drivers/dvb_usbci/amlogic-usb-cam.ko",
    "//common_drivers/drivers/dvb_usbci:amlogic-usb-cam",
    "common_drivers/drivers/dvb/demux/amlogic-dvb-demux.ko",
    "//common_drivers/drivers/dvb/demux:amlogic-dvb-demux",
    "common_drivers/drivers/dvb/dummy_fe/amlogic-dummy-fe.ko",
    "//common_drivers/drivers/dvb/dummy_fe:amlogic-dummy-fe",
    "common_drivers/drivers/efuse_unifykey/amlogic-efuse-unifykey.ko",
    "//common_drivers/drivers/efuse_unifykey:amlogic-efuse-unifykey",
    "common_drivers/drivers/gki_tool/amlogic-gkitool.ko",
    "//common_drivers/drivers/gki_tool:amlogic-gkitool",
    "common_drivers/drivers/gpio/amlogic-gpio.ko",
    "//common_drivers/drivers/gpio:amlogic-gpio",
    "common_drivers/drivers/gpio/amlogic-pinctrl-soc-g12a.ko",
    "//common_drivers/drivers/gpio:amlogic-pinctrl-soc-g12a",
    "common_drivers/drivers/gpio/amlogic-pinctrl-soc-s4.ko",
    "//common_drivers/drivers/gpio:amlogic-pinctrl-soc-s4",
    "common_drivers/drivers/gpio/amlogic-pinctrl-soc-s5.ko",
    "//common_drivers/drivers/gpio:amlogic-pinctrl-soc-s5",
    "common_drivers/drivers/gpio/amlogic-pinctrl-soc-s6.ko",
    "//common_drivers/drivers/gpio:amlogic-pinctrl-soc-s6",
    "common_drivers/drivers/gpio/amlogic-pinctrl-soc-s7.ko",
    "//common_drivers/drivers/gpio:amlogic-pinctrl-soc-s7",
    "common_drivers/drivers/gpio/amlogic-pinctrl-soc-s7d.ko",
    "//common_drivers/drivers/gpio:amlogic-pinctrl-soc-s7d",
    "common_drivers/drivers/gpio/amlogic-pinctrl-soc-sc2.ko",
    "//common_drivers/drivers/gpio:amlogic-pinctrl-soc-sc2",
    "common_drivers/drivers/gpio/amlogic-pinctrl-soc-t3.ko",
    "//common_drivers/drivers/gpio:amlogic-pinctrl-soc-t3",
    "common_drivers/drivers/gpio/amlogic-pinctrl-soc-t3x.ko",
    "//common_drivers/drivers/gpio:amlogic-pinctrl-soc-t3x",
    "common_drivers/drivers/gpio/amlogic-pinctrl-soc-t5m.ko",
    "//common_drivers/drivers/gpio:amlogic-pinctrl-soc-t5m",
    "common_drivers/drivers/gpio/amlogic-pinctrl-soc-t5w.ko",
    "//common_drivers/drivers/gpio:amlogic-pinctrl-soc-t5w",
    "common_drivers/drivers/gpio/amlogic-pinctrl-soc-t6d.ko",
    "//common_drivers/drivers/gpio:amlogic-pinctrl-soc-t6d",
    "common_drivers/drivers/gpio/amlogic-pinctrl-soc-t6w.ko",
    "//common_drivers/drivers/gpio:amlogic-pinctrl-soc-t6w",
    "common_drivers/drivers/gpio/amlogic-pinctrl-soc-t6x.ko",
    "//common_drivers/drivers/gpio:amlogic-pinctrl-soc-t6x",
    "common_drivers/drivers/gpio/amlogic-pinctrl-soc-t7.ko",
    "//common_drivers/drivers/gpio:amlogic-pinctrl-soc-t7",
    "common_drivers/drivers/gpio/amlogic-pinctrl-soc-tm2.ko",
    "//common_drivers/drivers/gpio:amlogic-pinctrl-soc-tm2",
    "common_drivers/drivers/host/amlogic-host.ko",
    "//common_drivers/drivers/host:amlogic-host",
    "common_drivers/drivers/hwspinlock/amlogic-hwspinlock.ko",
    "//common_drivers/drivers/hwspinlock:amlogic-hwspinlock",
    "common_drivers/drivers/i2c/busses/amlogic-i2c.ko",
    "//common_drivers/drivers/i2c/busses:amlogic-i2c",
    "common_drivers/drivers/iio/adc/amlogic-adc.ko",
    "//common_drivers/drivers/iio/adc:amlogic-adc",
    "common_drivers/drivers/input/amlogic-input.ko",
    "//common_drivers/drivers/input:amlogic-input",
    "common_drivers/drivers/irblaster/amlogic-irblaster.ko",
    "//common_drivers/drivers/irblaster:amlogic-irblaster",
    "common_drivers/drivers/jtag/amlogic-jtag.ko",
    "//common_drivers/drivers/jtag:amlogic-jtag",
    "common_drivers/drivers/led/amlogic-led.ko",
    "//common_drivers/drivers/led:amlogic-led",
    "common_drivers/drivers/mailbox/amlogic-mailbox.ko",
    "//common_drivers/drivers/mailbox:amlogic-mailbox",
    "common_drivers/drivers/media/dvb-core/dvb-core.ko",
    "//common_drivers/drivers/media/dvb-core:dvb-core",
    "common_drivers/drivers/media/aml_media.ko",
    "//common_drivers/drivers/media:aml_media",
    "common_drivers/drivers/memory_debug/amlogic-memory-debug.ko",
    "//common_drivers/drivers/memory_debug:amlogic-memory-debug",
    "common_drivers/drivers/memory_ext/aml_smmu.ko",
    "//common_drivers/drivers/memory_ext:aml_smmu",
    "common_drivers/drivers/memory_ext/user_fault.ko",
    "//common_drivers/drivers/memory_ext:user_fault",
    "common_drivers/drivers/mmc/host/amlogic-mmc.ko",
    "//common_drivers/drivers/mmc/host:amlogic-mmc",
    "common_drivers/drivers/net/ethernet/stmicro/stmmac/amlogic-phy-debug.ko",
    "//common_drivers/drivers/net:amlogic-phy-debug",
    "common_drivers/drivers/net/ethernet/stmicro/stmmac/dwmac-dwc-qos-eth.ko",
    "//common_drivers/drivers/net:dwmac-dwc-qos-eth",
    "common_drivers/drivers/net/ethernet/stmicro/stmmac/dwmac-meson.ko",
    "//common_drivers/drivers/net:dwmac-meson",
    "common_drivers/drivers/net/ethernet/stmicro/stmmac/dwmac-meson8b.ko",
    "//common_drivers/drivers/net:dwmac-meson8b",
    "common_drivers/drivers/net/ethernet/stmicro/stmmac/stmmac.ko",
    "//common_drivers/drivers/net:stmmac",
    "common_drivers/drivers/net/ethernet/stmicro/stmmac/stmmac-platform.ko",
    "//common_drivers/drivers/net:stmmac-platform",
    "common_drivers/drivers/net/mdio/amlogic-mdio-g12a.ko",
    "//common_drivers/drivers/net:amlogic-mdio-g12a",
    "common_drivers/drivers/net/phy/amlogic-inphy.ko",
    "//common_drivers/drivers/net:amlogic-inphy",
    "common_drivers/drivers/net/phy/amlogic-realtek.ko",
    "//common_drivers/drivers/net:amlogic-realtek",
    "common_drivers/drivers/pci/controller/amlogic-pcie.ko",
    "//common_drivers/drivers/pci/controller:amlogic-pcie",
    "common_drivers/drivers/pm/amlogic-pm.ko",
    "//common_drivers/drivers/pm:amlogic-pm",
    "common_drivers/drivers/power/amlogic-power.ko",
    "//common_drivers/drivers/power:amlogic-power",
    "common_drivers/drivers/pwm/amlogic-pwm.ko",
    "//common_drivers/drivers/pwm:amlogic-pwm",
    "common_drivers/drivers/regulator/amlogic-regulator.ko",
    "//common_drivers/drivers/regulator:amlogic-regulator",
    "common_drivers/drivers/reset/amlogic-reset.ko",
    "//common_drivers/drivers/reset:amlogic-reset",
    "common_drivers/drivers/rtc/amlogic-rtc.ko",
    "//common_drivers/drivers/rtc:amlogic-rtc",
    "common_drivers/drivers/secmon/amlogic-secmon.ko",
    "//common_drivers/drivers/secmon:amlogic-secmon",
    "common_drivers/drivers/soc_info/amlogic-socinfo.ko",
    "//common_drivers/drivers/soc_info:amlogic-socinfo",
    "common_drivers/drivers/spi/amlogic-spi.ko",
    "//common_drivers/drivers/spi:amlogic-spi",
    "common_drivers/drivers/tee/amlogic-tee.ko",
    "//common_drivers/drivers/tee:amlogic-tee",
    "common_drivers/drivers/thermal/amlogic-thermal.ko",
    "//common_drivers/drivers/thermal:amlogic-thermal",
    "common_drivers/drivers/usb/amlogic-usb.ko",
    "//common_drivers/drivers/usb:amlogic-usb",
    "common_drivers/drivers/watchdog/amlogic-watchdog.ko",
    "//common_drivers/drivers/watchdog:amlogic-watchdog",
    "common_drivers/drivers/wireless/amlogic-wireless.ko",
    "//common_drivers/drivers/wireless:amlogic-wireless",
    "common_drivers/drivers/seckey/amlogic-seckey.ko",
    "//common_drivers/drivers/seckey:amlogic-seckey",
    "common_drivers/sound/soc/codecs/amlogic/amlogic-snd-codec-ad82128.ko",
    "//common_drivers/sound/soc:amlogic-snd-codec-ad82128",
    "common_drivers/sound/soc/codecs/amlogic/amlogic-snd-codec-ad82584f.ko",
    "//common_drivers/sound/soc:amlogic-snd-codec-ad82584f",
    "common_drivers/sound/soc/codecs/amlogic/amlogic-snd-codec-dummy.ko",
    "//common_drivers/sound/soc:amlogic-snd-codec-dummy",
    "common_drivers/sound/soc/codecs/amlogic/amlogic-snd-codec-gxlx4.ko",
    "//common_drivers/sound/soc:amlogic-snd-codec-gxlx4",
    "common_drivers/sound/soc/codecs/amlogic/amlogic-snd-codec-pa1.ko",
    "//common_drivers/sound/soc:amlogic-snd-codec-pa1",
    "common_drivers/sound/soc/codecs/amlogic/amlogic-snd-codec-t9015.ko",
    "//common_drivers/sound/soc:amlogic-snd-codec-t9015",
    "common_drivers/sound/soc/codecs/amlogic/amlogic-snd-codec-tas5707.ko",
    "//common_drivers/sound/soc:amlogic-snd-codec-tas5707",
    "common_drivers/sound/soc/codecs/amlogic/amlogic-snd-codec-tas5805.ko",
    "//common_drivers/sound/soc:amlogic-snd-codec-tas5805",
    "common_drivers/sound/soc/codecs/amlogic/amlogic-snd-codec-tl1.ko",
    "//common_drivers/sound/soc:amlogic-snd-codec-tl1",
    "common_drivers/sound/soc/codecs/amlogic/amlogic-snd-codec-sy6026l.ko",
    "//common_drivers/sound/soc:amlogic-snd-codec-sy6026l",
    "common_drivers/sound/soc/codecs/amlogic/amlogic-snd-codec-t6d.ko",
    "//common_drivers/sound/soc:amlogic-snd-codec-t6d",
    "common_drivers/sound/soc/codecs/amlogic/amlogic-snd-codec-ad82120b.ko",
    "//common_drivers/sound/soc:amlogic-snd-codec-ad82120b",
    "common_drivers/sound/soc/amlogic/amlogic-snd-soc.ko",
    "//common_drivers/sound/soc:amlogic-snd-soc",
    "common_drivers/sound/soc/amlogic/auge/vad/amlogic-snd-vad.ko",
    "//common_drivers/sound/soc:amlogic-snd-vad",
    "sound/drivers/snd-aloop.ko",
]

AMLOGOC_COMMON_MODULES_REMOVE = (
    [
        "kernel/kheaders.ko",
        "drivers/android/binder/rust_binder.ko",
    ] + (
        [
            "common_drivers/drivers/tty/serial/amlogic-uart.ko",
            "//common_drivers/drivers/tty/serial:amlogic-uart",
        ] if project_configs.UPGRADE_PROJECT else []
    )
) if project_configs.GKI_CONFIG in ("", "non_gki") else (
    ["drivers/android/binder/rust_binder.ko"] if project_configs.KASAN else []
)

AMLOGIC_UPGRADE_COMMON_MODULES = [
    # keep sorted
    "arch/arm64/crypto/sha1-ce.ko",
    "drivers/i2c/i2c-dev.ko",
    "drivers/net/mdio/mdio-mux.ko",
    "drivers/net/pcs/pcs_xpcs.ko",
    "drivers/regulator/gpio-regulator.ko",
    "net/mac80211/mac80211.ko",
    "net/wireless/cfg80211.ko",
    "common_drivers/drivers/crypto/amlogic-crypto-dma.ko",
    "//common_drivers/drivers/crypto:amlogic-crypto-dma",
    "common_drivers/drivers/debug/amlogic-audio-utils.ko",
    "//common_drivers/drivers/debug:amlogic-audio-utils",
    "common_drivers/drivers/drm/aml_drm.ko",
    "//common_drivers/drivers/drm:aml_drm",
    "common_drivers/drivers/dvb_ci/amlogic-dvb-ci.ko",
    "//common_drivers/drivers/dvb_ci:amlogic-dvb-ci",
    "common_drivers/drivers/dvb_usbci/amlogic-usb-cam.ko",
    "//common_drivers/drivers/dvb_usbci:amlogic-usb-cam",
    "common_drivers/drivers/dvb/demux/amlogic-dvb-demux.ko",
    "//common_drivers/drivers/dvb/demux:amlogic-dvb-demux",
    "common_drivers/drivers/host/amlogic-host.ko",
    "//common_drivers/drivers/host:amlogic-host",
    "common_drivers/drivers/irblaster/amlogic-irblaster.ko",
    "//common_drivers/drivers/irblaster:amlogic-irblaster",
    "common_drivers/drivers/jtag/amlogic-jtag.ko",
    "//common_drivers/drivers/jtag:amlogic-jtag",
    "common_drivers/drivers/led/amlogic-led.ko",
    "//common_drivers/drivers/led:amlogic-led",
    "common_drivers/drivers/net/mdio/amlogic-mdio-g12a.ko",
    "//common_drivers/drivers/net:amlogic-mdio-g12a",
    "common_drivers/drivers/net/phy/amlogic-inphy.ko",
    "//common_drivers/drivers/net:amlogic-inphy",
    "common_drivers/drivers/pci/controller/amlogic-pcie.ko",
    "//common_drivers/drivers/pci/controller:amlogic-pcie",
    "common_drivers/drivers/rtc/amlogic-rtc.ko",
    "//common_drivers/drivers/rtc:amlogic-rtc",
    "common_drivers/drivers/soc_info/amlogic-socinfo.ko",
    "//common_drivers/drivers/soc_info:amlogic-socinfo",
    "common_drivers/drivers/usb/amlogic-usb.ko",
    "//common_drivers/drivers/usb:amlogic-usb",
    "common_drivers/drivers/wireless/amlogic-wireless.ko",
    "//common_drivers/drivers/wireless:amlogic-wireless",
    "common_drivers/drivers/seckey/amlogic-seckey.ko",
    "//common_drivers/drivers/seckey:amlogic-seckey",
    "common_drivers/drivers/net/ethernet/stmicro/stmmac/amlogic-phy-debug.ko",
    "//common_drivers/drivers/net:amlogic-phy-debug",
    "common_drivers/drivers/net/ethernet/stmicro/stmmac/dwmac-dwc-qos-eth.ko",
    "//common_drivers/drivers/net:dwmac-dwc-qos-eth",
    "common_drivers/drivers/net/ethernet/stmicro/stmmac/dwmac-meson.ko",
    "//common_drivers/drivers/net:dwmac-meson",
    "common_drivers/drivers/net/ethernet/stmicro/stmmac/dwmac-meson8b.ko",
    "//common_drivers/drivers/net:dwmac-meson8b",
    "common_drivers/drivers/net/ethernet/stmicro/stmmac/stmmac.ko",
    "//common_drivers/drivers/net:stmmac",
    "common_drivers/drivers/net/ethernet/stmicro/stmmac/stmmac-platform.ko",
    "//common_drivers/drivers/net:stmmac-platform",
    "common_drivers/sound/soc/codecs/amlogic/amlogic-snd-codec-ad82128.ko",
    "//common_drivers/sound/soc:amlogic-snd-codec-ad82128",
    "common_drivers/sound/soc/codecs/amlogic/amlogic-snd-codec-ad82584f.ko",
    "//common_drivers/sound/soc:amlogic-snd-codec-ad82584f",
    "common_drivers/sound/soc/codecs/amlogic/amlogic-snd-codec-dummy.ko",
    "//common_drivers/sound/soc:amlogic-snd-codec-dummy",
    "common_drivers/sound/soc/codecs/amlogic/amlogic-snd-codec-gxlx4.ko",
    "//common_drivers/sound/soc:amlogic-snd-codec-gxlx4",
    "common_drivers/sound/soc/codecs/amlogic/amlogic-snd-codec-pa1.ko",
    "//common_drivers/sound/soc:amlogic-snd-codec-pa1",
    "common_drivers/sound/soc/codecs/amlogic/amlogic-snd-codec-t9015.ko",
    "//common_drivers/sound/soc:amlogic-snd-codec-t9015",
    "common_drivers/sound/soc/codecs/amlogic/amlogic-snd-codec-tas5707.ko",
    "//common_drivers/sound/soc:amlogic-snd-codec-tas5707",
    "common_drivers/sound/soc/codecs/amlogic/amlogic-snd-codec-tas5805.ko",
    "//common_drivers/sound/soc:amlogic-snd-codec-tas5805",
    "common_drivers/sound/soc/codecs/amlogic/amlogic-snd-codec-tl1.ko",
    "//common_drivers/sound/soc:amlogic-snd-codec-tl1",
    "common_drivers/sound/soc/codecs/amlogic/amlogic-snd-codec-sy6026l.ko",
    "//common_drivers/sound/soc:amlogic-snd-codec-sy6026l",
    "common_drivers/sound/soc/codecs/amlogic/amlogic-snd-codec-t6d.ko",
    "//common_drivers/sound/soc:amlogic-snd-codec-t6d",
    "common_drivers/sound/soc/codecs/amlogic/amlogic-snd-codec-ad82120b.ko",
    "//common_drivers/sound/soc:amlogic-snd-codec-ad82120b",
    "common_drivers/sound/soc/amlogic/amlogic-snd-soc.ko",
    "//common_drivers/sound/soc:amlogic-snd-soc",
    "common_drivers/drivers/dvb/dummy_fe/amlogic-dummy-fe.ko",
    "//common_drivers/drivers/dvb/dummy_fe:amlogic-dummy-fe",
    "common_drivers/drivers/amfc/amlogic-soc-amfc.ko",
    "//common_drivers/drivers/amfc:amlogic-soc-amfc",
    "common_drivers/drivers/amfc/aml_erofs/amlogic-soc-erofs.ko",
    "//common_drivers/drivers/amfc:amlogic-soc-erofs",
    "common_drivers/drivers/amfc/aml_zram/aml-zram.ko",
    "//common_drivers/drivers/amfc:aml-zram",
    "common_drivers/drivers/amfc/sym_helper/amlogic-soc-sym_helper.ko",
    "//common_drivers/drivers/amfc:amlogic-soc-sym_helper",
    "common_drivers/drivers/aml_watermark/aml_watermark.ko",
    "//common_drivers/drivers/aml_watermark:aml_watermark",
    "common_drivers/drivers/net/phy/amlogic-realtek.ko",
    "//common_drivers/drivers/net:amlogic-realtek",
]

AMLOGIC_UPGRADE_P_MODULES = [

]

AMLOGIC_UPGRADE_R_MODULES = [

]

AMLOGIC_UPGRADE_S_MODULES = [

]

AMLOGIC_UPGRADE_U_MODULES = [

]

AMLOGIC_UPGRADE_R_REMOVE_MODULES = [
    "kernel/kheaders.ko",
]

AMLOGIC_UPGRADE_S_REMOVE_MODULES = [
]

AMLOGIC_UPGRADE_P_REMOVE_MODULES = [
    "drivers/net/mdio/mdio-mux.ko",
    "drivers/net/pcs/pcs_xpcs.ko",
    "drivers/net/mii.ko",
    "kernel/kheaders.ko",
    "common_drivers/drivers/drm/aml_drm.ko",
    "//common_drivers/drivers/drm:aml_drm",
    "common_drivers/drivers/hwspinlock/amlogic-hwspinlock.ko",
    "//common_drivers/drivers/hwspinlock:amlogic-hwspinlock",
    "common_drivers/drivers/iio/adc/amlogic-adc.ko",
    "//common_drivers/drivers/iio/adc:amlogic-adc",
    "common_drivers/drivers/irblaster/amlogic-irblaster.ko",
    "//common_drivers/drivers/irblaster:amlogic-irblaster",
    "common_drivers/drivers/net/mdio/amlogic-mdio-g12a.ko",
    "//common_drivers/drivers/net:amlogic-mdio-g12a",
    "common_drivers/drivers/net/phy/amlogic-inphy.ko",
    "//common_drivers/drivers/net:amlogic-inphy",
    "common_drivers/drivers/net/phy/amlogic-realtek.ko",
    "//common_drivers/drivers/net:amlogic-realtek",
    "common_drivers/drivers/thermal/amlogic-thermal.ko",
    "//common_drivers/drivers/thermal:amlogic-thermal",
    "common_drivers/drivers/usb/amlogic-usb.ko",
    "//common_drivers/drivers/usb:amlogic-usb",
    "common_drivers/drivers/net/ethernet/stmicro/stmmac/amlogic-phy-debug.ko",
    "//common_drivers/drivers/net:amlogic-phy-debug",
    "common_drivers/drivers/net/ethernet/stmicro/stmmac/dwmac-dwc-qos-eth.ko",
    "//common_drivers/drivers/net:dwmac-dwc-qos-eth",
    "common_drivers/drivers/net/ethernet/stmicro/stmmac/dwmac-meson.ko",
    "//common_drivers/drivers/net:dwmac-meson",
    "common_drivers/drivers/net/ethernet/stmicro/stmmac/dwmac-meson8b.ko",
    "//common_drivers/drivers/net:dwmac-meson8b",
    "common_drivers/drivers/net/ethernet/stmicro/stmmac/stmmac.ko",
    "//common_drivers/drivers/net:stmmac",
    "common_drivers/drivers/net/ethernet/stmicro/stmmac/stmmac-platform.ko",
    "//common_drivers/drivers/net:stmmac-platform",
]

AMLOGIC_UPGRADE_U_REMOVE_MODULES = [
]

AMLOGIC_DDK_MODULES = []

def define_modules(
    project_configs = None
    ):

    AMLOGIC_GKIX_MODULES = AMLOGIC_GKI20_MODULES if project_configs.GKI_CONFIG == "gki_20" else AMLOGIC_GKI10_MODULES
    AMLOGIC_COMMON_OR_UPGRADE_MODULES = AMLOGIC_UPGRADE_COMMON_MODULES + AMLOGIC_UPGRADE_R_MODULES if project_configs.UPGRADE_PROJECT == "r" or project_configs.UPGRADE_PROJECT == "R" else \
        AMLOGIC_UPGRADE_COMMON_MODULES + AMLOGIC_UPGRADE_S_MODULES if project_configs.UPGRADE_PROJECT == "s" or project_configs.UPGRADE_PROJECT == "S" else \
        AMLOGIC_UPGRADE_COMMON_MODULES + AMLOGIC_UPGRADE_P_MODULES if project_configs.UPGRADE_PROJECT == "p" or project_configs.UPGRADE_PROJECT == "P" else \
        AMLOGIC_UPGRADE_COMMON_MODULES + AMLOGIC_UPGRADE_U_MODULES if project_configs.UPGRADE_PROJECT == "u" or project_configs.UPGRADE_PROJECT == "U" else \
	AMLOGIC_UPGRADE_COMMON_MODULES if project_configs.UPGRADE_PROJECT else \
        AMLOGIC_COMMON_MODULES
    AMLOGIC_PROJECT_ALL_MODULES = AMLOGIC_COMMON_OR_UPGRADE_MODULES + AMLOGIC_GKIX_MODULES + project_configs.MODULES_OUT_ADD + AMLOGIC_ANDROID_MODULES if project_configs.EXTRA_ANDROID_MODULE else \
        AMLOGIC_COMMON_OR_UPGRADE_MODULES + AMLOGIC_GKIX_MODULES + project_configs.MODULES_OUT_ADD
    AMLOGIC_UNIQUE_PROJECT_ALL_MODULES = [k for k in {x: None for x in AMLOGIC_PROJECT_ALL_MODULES}]

    AMLOGIC_PROJECT_REMOVE_MODULES = \
        AMLOGOC_COMMON_MODULES_REMOVE + project_configs.MODULES_OUT_REMOVE + AMLOGIC_UPGRADE_R_REMOVE_MODULES if project_configs.UPGRADE_PROJECT == "r" or project_configs.UPGRADE_PROJECT == "R" else \
        AMLOGOC_COMMON_MODULES_REMOVE + project_configs.MODULES_OUT_REMOVE + AMLOGIC_UPGRADE_S_REMOVE_MODULES if project_configs.UPGRADE_PROJECT == "s" or project_configs.UPGRADE_PROJECT == "S" else \
        AMLOGOC_COMMON_MODULES_REMOVE + project_configs.MODULES_OUT_REMOVE + AMLOGIC_UPGRADE_P_REMOVE_MODULES if project_configs.UPGRADE_PROJECT == "p" or project_configs.UPGRADE_PROJECT == "P" else \
        AMLOGOC_COMMON_MODULES_REMOVE + project_configs.MODULES_OUT_REMOVE + AMLOGIC_UPGRADE_U_REMOVE_MODULES if project_configs.UPGRADE_PROJECT == "u" or project_configs.UPGRADE_PROJECT == "U" else \
        AMLOGOC_COMMON_MODULES_REMOVE + project_configs.MODULES_OUT_REMOVE
    AMLOGIC_UNIQUE_PROJECT_REMOVE_MODULES = [k for k in {x: None for x in AMLOGIC_PROJECT_REMOVE_MODULES}]
    AMLOGIC_REMOVE_KERNEL_MODULES = [module for module in depset(AMLOGIC_UNIQUE_PROJECT_REMOVE_MODULES).to_list() if "common_drivers" not in module]

    AMLOGIC_PROJECT_MODULES = [i for i in AMLOGIC_UNIQUE_PROJECT_ALL_MODULES if i not in AMLOGIC_UNIQUE_PROJECT_REMOVE_MODULES]

    AMLOGIC_ALL_MODULES = [module for module in depset(AMLOGIC_PROJECT_MODULES).to_list() if ":" not in module]

    AMLOGIC_KERNEL_MODULES = [module for module in depset(AMLOGIC_PROJECT_MODULES).to_list() if "common_drivers" not in module]

    AMLOGIC_ALL_DDK_MODULES = [module for module in depset(AMLOGIC_PROJECT_MODULES).to_list() if ":" in module]

    AMLOGIC_MODULES = AMLOGIC_KERNEL_MODULES if project_configs.DDK_BUILD else AMLOGIC_ALL_MODULES
    AMLOGIC_DDK_MODULES = AMLOGIC_ALL_DDK_MODULES if project_configs.DDK_BUILD else []

    modules = struct (
        AMLOGIC_REMOVE_KERNEL_MODULES = AMLOGIC_REMOVE_KERNEL_MODULES,
        AMLOGIC_MODULES = AMLOGIC_MODULES,
        AMLOGIC_DDK_MODULES = AMLOGIC_DDK_MODULES,
    )

    return modules

DEFAULT_MODULES = define_modules(project_configs)
AMLOGIC_REMOVE_KERNEL_MODULES = DEFAULT_MODULES.AMLOGIC_REMOVE_KERNEL_MODULES
