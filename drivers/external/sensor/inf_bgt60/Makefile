#for Radar inf_bgt60 Driver
# obj-$(CONFIG_SEI_INF_BGT60) += sei_inf_bgt60.o

MODULE_NAME = sei_inf_bgt60
obj-$(CONFIG_SEI_INF_BGT60) += $(MODULE_NAME).o

PR_FMT = $(subst sei_,,$(MODULE_NAME))
PR_FMT_DEFINE = -Dpr_fmt(fmt)="[$(PR_FMT)]: " fmt

ccflags-y += $(PR_FMT_DEFINE)

