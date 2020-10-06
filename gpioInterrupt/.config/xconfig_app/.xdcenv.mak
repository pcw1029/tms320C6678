#
_XDCBUILDCOUNT = 
ifneq (,$(findstring path,$(_USEXDCENV_)))
override XDCPATH = C:/ti/bios_6_76_03_01/packages;C:/ti/pdk_c667x_2_0_16/packages;C:/MulticoreDSP/dsp_workspace/gpioInterrupt/.config
override XDCROOT = C:/ti/ccs930/xdctools_3_60_02_34_core
override XDCBUILDCFG = ./config.bld
endif
ifneq (,$(findstring args,$(_USEXDCENV_)))
override XDCARGS = 
override XDCTARGETS = 
endif
#
ifeq (0,1)
PKGPATH = C:/ti/bios_6_76_03_01/packages;C:/ti/pdk_c667x_2_0_16/packages;C:/MulticoreDSP/dsp_workspace/gpioInterrupt/.config;C:/ti/ccs930/xdctools_3_60_02_34_core/packages;..
HOSTOS = Windows
endif
