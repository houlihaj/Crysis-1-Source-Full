#############################################################################
## Crytek Source File
## Copyright (C) 2006, Crytek Studios
##
## Creator: Sascha Demetrio
## Date: Jul 31, 2006
## Description: GNU-make based build system
#############################################################################

PROJECT_TYPE := module
PROJECT_VCPROJ := CryScriptSystem.vcproj

PROJECT_PS3_PRX := 1
ifeq ($(ARCH),PS3-cell)
 PROJECT_STUB_SOURCES_CPP := CryScriptSystem.cpp
 PROJECT_LDLIBS := -lpthread -lm_stub -lfs_stub -lnet_stub
endif

PROJECT_LINKMODULES := CrySystem

PROJECT_CPPFLAGS_COMMON := \
	-I$(CODE_ROOT)/CryEngine/CryCommon \
	-I$(PROJECT_CODE)/Lua

PROJECT_SOURCES_CPP_REMOVE := StdAfx.cpp

ifeq ($(OPTION_PROFILE),1)
 PROJECT_XFLAGS := -O3 -fno-schedule-insns2 -fno-schedule-insns 
endif
