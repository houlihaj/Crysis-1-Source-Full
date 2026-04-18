#############################################################################
## Crytek Source File
## Copyright (C) 2006, Crytek Studios
##
## Creator: Sascha Demetrio
## Date: Jul 31, 2006
## Description: GNU-make based build system
#############################################################################

PROJECT_TYPE := module
PROJECT_VCPROJ := CryFont.vcproj

PROJECT_PS3_PRX := 1
ifeq ($(ARCH),PS3-cell)
 PROJECT_STUB_SOURCES_CPP := ICryFont.cpp
endif

PROJECT_CPPFLAGS_COMMON := \
	-I$(CODE_ROOT)/CryEngine/CryCommon \
	-I$(PROJECT_CODE)/FreeType2/include

ifeq ($(ARCH),PS3-cell)
 PROJECT_PS3_PRX_OK := 1
 PROJECT_LDLIBS := -lpthread -lm_stub -lfs_stub -lnet_stub
endif

PROJECT_LINKMODULES := CrySystem

PROJECT_SOURCES_CPP_REMOVE := StdAfx.cpp

# vim:ts=8:sw=8

