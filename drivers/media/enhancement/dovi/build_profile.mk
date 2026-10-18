# STB 2.6 host profile and the vendor 5.15 Clang/LLVM interface.
DOVI_MODULE_NAME := dovi
LLVM ?= 1
DOVI_KERNEL_OPTIONS := LLVM=$(LLVM) LLVM_IAS=1
COMMON_DRIVERS ?= $(src)/../../../..
ccflags-y += -I$(COMMON_DRIVERS)/include
ccflags-y += -I$(COMMON_DRIVERS)/drivers/media/enhancement/amdolby_vision
ccflags-y += -fno-strict-aliasing -fwrapv
