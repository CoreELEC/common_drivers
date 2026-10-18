include build_profile.mk
KDIR ?= /lib/modules/$(shell uname -r)/build
KOUT ?=
ARCH ?= arm64

.PHONY: all clean
all clean:
	$(MAKE) -C $(KDIR) $(if $(KOUT),O=$(KOUT)) M=$(CURDIR) ARCH=$(ARCH) $(DOVI_KERNEL_OPTIONS) $(if $(filter clean,$@),clean,modules)
