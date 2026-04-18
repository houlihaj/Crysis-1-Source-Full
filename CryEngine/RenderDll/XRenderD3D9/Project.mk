#############################################################################
## Crytek Source File
## Copyright (C) 2006, Crytek Studios
##
## Creator: Sascha Demetrio
## Date: Jul 31, 2006
## Description: GNU-make based build system
#############################################################################

PROJECT_TYPE := module
PROJECT_VCPROJ := XRenderD3D10.vcproj

PROJECT_PS3_PRX := 1
ifeq ($(ARCH),PS3-cell)
 PROJECT_STUB_SOURCES_CPP := DriverD3D.cpp
endif

PROJECT_LINKMODULES := CrySystem

PROJECT_CPPFLAGS_COMMON := \
	-I$(CODE_ROOT)/CryEngine/CryCommon \
	-I$(PROJECT_CODE)/.. \
	-I$(PROJECT_BUILD)/.. \
	-DENABLE_FRAME_PROFILER -DDO_RENDERLOG -DDO_RENDERSTATS -D_RENDERER

ifeq ($(ARCH),PS3-cell)
 PROJECT_CPPFLAGS_COMMON += \
	-DXRENDERD3D10_EXPORTS -DDIRECT3D10 -DCRY_USE_GCM
 PROJECT_LDLIBS := -lpthread -lcgc
 ifeq ($(OPTION_PS3_GCMHUD),1)
  PROJECT_LDLIBS += -lgcm_hud -lgcm_pm 
 else
  PROJECT_LDLIBS += -lgcm_cmd -lgcm_sys_stub
 endif
 PROJECT_LDLIBS += -lm_stub -lfs_stub -lnet_stub -lsysutil_stub
endif # ARCH == PS3-cell

PROJECT_SOURCES_CPP_REMOVE := ../RenderPCH.cpp

ifeq ($(OPTION_PROFILE),1)
 ifeq ($(OPTION_WHOLE_PROJECT),1)
  PROJECT_XFLAGS := -O3 -fno-schedule-insns2 -fno-schedule-insns 
 else
  PROJECT_XFLAGS := -O3
  PROJECT_XFLAGS_ShaderFXParseBin_cpp := -O3 -fno-schedule-insns2 -fno-schedule-insns 
 endif
endif

ifeq ($(OPTION_WHOLE_PROJECT),1)
 PROJECT_CFLAGS += -DWHOLE_PROGRAM
endif

# vim:ts=8:sw=8

