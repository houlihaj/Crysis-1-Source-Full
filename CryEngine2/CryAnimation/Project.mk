#############################################################################
## Crytek Source File
## Copyright (C) 2006, Crytek Studios
##
## Creator: Sascha Demetrio
## Date: Jul 31, 2006
## Description: GNU-make based build system
#############################################################################

PROJECT_TYPE := module
PROJECT_VCPROJ := CryAnimation.vcproj

PROJECT_PS3_PRX := 1
ifeq ($(ARCH),PS3-cell)
 PROJECT_STUB_SOURCES_CPP := AnimationBase.cpp
 PROJECT_LDLIBS := -lpthread -lm_stub -lfs_stub -lnet_stub
endif

PROJECT_LINKMODULES := CrySystem

PROJECT_CPPFLAGS_COMMON := \
	-I$(CODE_ROOT)/CryEngine/CryCommon

PROJECT_SOURCES_CPP_REMOVE := stdafx.cpp

ifeq ($(OPTION_PROFILE),1)
 PROJECT_XFLAGS := -O3
endif

# vim:ts=8:sw=8

