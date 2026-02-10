#for Radar inf_bgt60 Driver
# obj-$(CONFIG_SEI_INF_BGT60) += sei_inf_bgt60.o

MODULE_NAME = sei_inf_bgt60
obj-$(CONFIG_SEI_INF_BGT60) += $(MODULE_NAME).o
$(MODULE_NAME)-y += sei_inf_bgt60.o


# for [sei_nta533_nfc] kernel log prefix
PR_FMT = $(subst sei-,,$(MODULE_NAME))
PR_FMT_DEFINE="-Dpr_fmt(fmt)= \"[$(PR_FMT)]: \" fmt"
ccflags-y += $(PR_FMT_DEFINE)

