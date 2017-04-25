#
_XDCBUILDCOUNT = 0
ifneq (,$(findstring path,$(_USEXDCENV_)))
override XDCPATH = C:/ti/bios_6_35_01_29/packages;C:/ti/ccsv5/ccs_base;R:/Newfolder/ece3849d17_lab2_atlippert_gholman/.config
override XDCROOT = C:/ti/xdctools_3_25_00_48
override XDCBUILDCFG = ./config.bld
endif
ifneq (,$(findstring args,$(_USEXDCENV_)))
override XDCARGS = 
override XDCTARGETS = 
endif
#
ifeq (0,1)
PKGPATH = C:/ti/bios_6_35_01_29/packages;C:/ti/ccsv5/ccs_base;R:/Newfolder/ece3849d17_lab2_atlippert_gholman/.config;C:/ti/xdctools_3_25_00_48/packages;..
HOSTOS = Windows
endif
