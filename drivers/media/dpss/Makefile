# SPDX-License-Identifier: (GPL-2.0+ OR MIT)

# Makefile for the Post Process Manager device #
#ifeq ($(TARGET_BUILD_VARIANT),userdebug)
#ccflags-y += -D DEBUG_SUPPORT
#ccflags-y += -DDEBUG
#else
#ccflags-y += -DDEBUG
#endif

ccflags-y += -I.

DI_CHANNEL_NUM_1?=0
DI_CHANNEL_NUM_2?=0

#CFLAGS_dim.o := -I$(src)
ifndef CONFIG_AMLOGIC_ZAPPER_CUT
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/c_common.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/dpss_hw.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/dpss_hw_frc.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/dpss_hw_nr.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/dpss_itf.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/dpss_s_common.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/dpss_s_frc.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/dpss_frc_reg.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/dpss_frc_dbg.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/dpss_frc_memc_dbg.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/dpss_s_nr.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/dpss_sys.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/dpss_rdma.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/dpss_input_q.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/dpss_rdma_frc.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/hw/decontour.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/hw/dpss.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/hw/dpss_dpe.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/hw/dpss_proc_irq_base.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/hw/dpss_proc_irq_tbc_sw_base.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/hw/dpss_vdi.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/hw/dpss_inp.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/hw/dpss_phslut.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/hw/fgrain.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/hw/dpss_aa_pps.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/hw/dpss_intf.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/hw/dpss_ro_check.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/hw/dpss_lib.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/hw/dpss_sw_cfg.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/hw/vfcd.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/hw/dpss_dae.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/hw/dpss_mc.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/hw/vfce.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/drv/dpss_pq_drv.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/drv/d_pq_mgr.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/drv/d_dd.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/drv/d_hdr.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_MEDIA_DPSS) += dpss/drv/d_pq.o
ccflags-y += -I$(COMMON_DRIVERS_DIR)/drivers/media/common/rdma/
ccflags-y += -I$(src)/dpss/
ccflags-y += -I$(src)/dpss/drv/
ccflags-y += -I$(src)/dpss/hw/
ccflags-y += -DCOMMON_DRIVERS_DIR=$(COMMON_DRIVERS_DIR)
ccflags-y += "-DCONFIG_DI_CHANNEL_NUM_1=$(DI_CHANNEL_NUM_1)"
ccflags-y += "-DCONFIG_DI_CHANNEL_NUM_2=$(DI_CHANNEL_NUM_2)"
endif
