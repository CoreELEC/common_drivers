# SPDX-License-Identifier: (GPL-2.0+ OR MIT)

MESON_DRM_MODULE_NAME = aml_drm
obj-$(CONFIG_AMLOGIC_DRM) = $(MESON_DRM_MODULE_NAME).o
$(MESON_DRM_MODULE_NAME)-y = meson_drm_main.o

ccflags-y += -I$(srctree)/$(src)/vpu-hw -I$(srctree)/$(src) \
	     -I$(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media \
	     -I$(srctree)/include

ifeq ($(CONFIG_AMLOGIC_DRM_USE_ION),y)
        $(MESON_DRM_MODULE_NAME)-y += meson_gem.o meson_fb.o
	ccflags-y += -I$(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/common/ion_dev
endif

ifeq ($(CONFIG_AMLOGIC_DRM_EMULATE_FBDEV),y)
        $(MESON_DRM_MODULE_NAME)-y += meson_fbdev.o
endif

ifneq ($(CONFIG_AMLOGIC_DRM_VPU),)
        $(MESON_DRM_MODULE_NAME)-y += meson_vpu.o
endif

ifeq ($(CONFIG_AMLOGIC_DRM_CUT_HDMI),)
        $(MESON_DRM_MODULE_NAME)-y += meson_hdmi.o
        $(MESON_DRM_MODULE_NAME)-y += meson_hdmitx_diag.o
endif

ifeq ($(CONFIG_AMLOGIC_DRM_DP),y)
        $(MESON_DRM_MODULE_NAME)-y += meson_dptx.o
	$(MESON_DRM_MODULE_NAME)-y += meson_tx_helper.o
endif

ifeq ($(CONFIG_AMLOGIC_DRM_EDP),y)
        $(MESON_DRM_MODULE_NAME)-y += meson_edp_modern.o
	$(MESON_DRM_MODULE_NAME)-y += meson_tx_helper.o
endif

ifeq ($(CONFIG_AMLOGIC_DRM_CUT_HDMI_MODERN),)
        $(MESON_DRM_MODULE_NAME)-y += meson_hdmi_modern.o
        $(MESON_DRM_MODULE_NAME)-y += meson_hdmitx_diag_modern.o
        $(MESON_DRM_MODULE_NAME)-y += meson_tx_helper.o
endif

ifeq ($(CONFIG_AMLOGIC_DRM_CUT_CVBS),)
        $(MESON_DRM_MODULE_NAME)-y += meson_cvbs.o
endif

$(MESON_DRM_MODULE_NAME)-y += meson_drv.o meson_plane.o \
                              meson_atomic.o  meson_sysfs.o\
                              meson_writeback.o meson_logo.o\
                              meson_async_atomic.o meson_of_parser.o \
                              meson_vpu_pipeline_traverse.o \
                              meson_crtc.o meson_vpu_pipeline.o \
                              meson_vpu_pipeline_private.o \
                              meson_debugfs.o meson_vpu_util.o \
                              meson_drm_bind.o\
                              meson_dummy.o \
                              meson_drm_rdma.o \
                              meson_venc.o \
                              meson_user_fence.o \
                              vpu-hw/meson_vpu_video_wrapper.o \
                              vpu-hw/meson_vpu_osd_mif.o \
                              vpu-hw/meson_osd_gfcd.o \
                              vpu-hw/meson_osd_afbc.o \
                              vpu-hw/meson_osd_scaler.o \
                              vpu-hw/meson_osd_proc.o \
                              vpu-hw/meson_vpu_osdblend.o \
                              vpu-hw/meson_vpu_hdr_dv.o \
                              vpu-hw/meson_osd_csc.o \
                              vpu-hw/meson_vpu_postblend.o \

ifeq ($(CONFIG_AMLOGIC_ZAPPER_CUT),)
        $(MESON_DRM_MODULE_NAME)-y += meson_lcd.o
endif

ifeq ($(CONFIG_AMLOGIC_ZAPPER_CUT),)
        $(MESON_DRM_MODULE_NAME)-y += meson_eDP.o
        # $(MESON_DRM_MODULE_NAME)-y += meson_eDP_aux.o
        # $(MESON_DRM_MODULE_NAME)-y += meson_eDP_backlight.o
endif
