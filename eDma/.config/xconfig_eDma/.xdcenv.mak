#
_XDCBUILDCOUNT = 
ifneq (,$(findstring path,$(_USEXDCENV_)))
override XDCPATH = C:/ti/pdk_c667x_2_0_16/packages;C:/ti/edma3_lld_2_12_05_30E/packages;C:/ti/bios_6_76_03_01/packages;C:/MulticoreDSP/dsp_workspace/eDma/.config
override XDCROOT = C:/ti/ccs930/xdctools_3_60_02_34_core
override XDCBUILDCFG = ./config.bld
endif
ifneq (,$(findstring args,$(_USEXDCENV_)))
override XDCARGS = 
override XDCTARGETS = 
endif
#
ifeq (0,1)
PKGPATH = C:/ti/pdk_c667x_2_0_16/packages;C:/ti/edma3_lld_2_12_05_30E/packages;C:/ti/bios_6_76_03_01/packages;C:/MulticoreDSP/dsp_workspace/eDma/.config;C:/ti/ccs930/xdctools_3_60_02_34_core/packages;..
HOSTOS = Windows
endif
