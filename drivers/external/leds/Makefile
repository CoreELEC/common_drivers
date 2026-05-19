obj-$(CONFIG_LEDS_AW21036) = leds-aw21036.o

# for [aw21036_led] kernel log prefix
PR_FMT = $(subst sei-,,$(MODULE_NAME))
PR_FMT_DEFINE="-Dpr_fmt(fmt)= \"[$(PR_FMT)]: \" fmt"
ccflags-y += $(PR_FMT_DEFINE)
