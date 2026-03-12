# SPDX-License-Identifier: (GPL-2.0+ OR MIT)

ccflags-y += \
	-DAW_HAVE_SNK \
	-DAW_HAVE_SRC \
	-DAW_HAVE_DRP \
	-DAW_HAVE_VBUS_ONLY \
	-DAW_HAVE_ACCMODE \
	-DAW_HAVE_EXT_MSG \
	-DAW_HAVE_VDM \
	-DAW_DEBUG_CODE \
	-DAW_HAVE_DP \
	-DAW_HAVE_PD \
	-DMESON_AW

obj-$(CONFIG_TYPEC_AW35615) += typec-aw35615.o

typec-aw35615-$(CONFIG_TYPEC_AW35615_WHOLE) := \
	aw35615_driver.o \
	aw35615_global.o \
	platform_helpers.o \
	dfs.o \
	core.o \
	PDPolicy.o \
	timer.o \
	PDProtocol.o \
	TypeC.o \
	Port.o \
	vendor_info.o \
	modules/dpm.o \
	modules/observer.o \
	vdm/bitfield_translators.o \
	vdm/vdm.o \
	vdm/vdm_callbacks.o \
	vdm/DisplayPort/dp.o

typec-aw35615-$(CONFIG_TYPEC_AW35615_TCPM) := tcpci_aw35615.o
