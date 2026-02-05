#for AW2016 LED Driver
# obj-$(CONFIG_SEI_AW2016_LED) += aw2016_led.o

MODULE_NAME = sei_ucs11681_light
obj-$(CONFIG_SEI_UCS11681_LIGHT) += $(MODULE_NAME).o
$(MODULE_NAME)-y += ucs11681_light.o


# for [sei_ucs11681_light] kernel log prefix
PR_FMT = $(subst sei-,,$(MODULE_NAME))
PR_FMT_DEFINE="-Dpr_fmt(fmt)= \"[$(PR_FMT)]: \" fmt"
ccflags-y += $(PR_FMT_DEFINE)

