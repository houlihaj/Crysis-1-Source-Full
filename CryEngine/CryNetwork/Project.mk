#############################################################################
## Crytek Source File
## Copyright (C) 2006, Crytek Studios
##
## Creator: Sascha Demetrio
## Date: Jul 31, 2006
## Description: GNU-make based build system
#############################################################################

PROJECT_TYPE := module
PROJECT_VCPROJ := CryNetwork.vcproj

PROJECT_PS3_PRX := 1
ifeq ($(ARCH),PS3-cell)
 PROJECT_STUB_SOURCES_CPP := CryNetwork.cpp
endif

PROJECT_LINKMODULES := CrySystem

PROJECT_CPPFLAGS := \
	-I$(CODE_ROOT)/CryEngine/CryCommon \
	-I$(CODE_ROOT)/SDKs \
	-I$(CODE_ROOT)/SDKs/Speex/include \
	-I$(CODE_ROOT)/SDKs/GameSpy/common

ifeq ($(ARCH),PS3-cell)
 # Exclude GameSpy.
 PROJECT_SOURCES_C_REMOVE := ../../SDKs/GameSpy/%
 PROJECT_SOURCES_CPP_REMOVE := Services/GameSpy/% ../../SDKs/GameSpy/% StdAfx.cpp
 PROJECT_SOURCES_H_REMOVE := ../../SDKs/GameSpy/% Services/GameSpy/%
 PROJECT_SOURCES_DIRS_REMOVE := ../../SDKs/GameSpy%

 PROJECT_LDLIBS := -lpthread -lm_stub -lfs_stub -lnet_stub
endif
ifeq ($(ARCH),Linux-x86)
 # Exclude GameSpy.
 PROJECT_SOURCES_C_REMOVE := ../../SDKs/GameSpy/%
 PROJECT_SOURCES_CPP_REMOVE := Services/GameSpy/% ../../SDKs/GameSpy/%
 PROJECT_SOURCES_H_REMOVE := ../../SDKs/GameSpy/% Services/GameSpy/%
 PROJECT_SOURCES_DIRS_REMOVE := ../../SDKs/GameSpy%

 PROJECT_CPPFLAGS += -D_LINUX
endif

# These will be needed once we're adding GameSpy to the build:
#ifeq ($(ARCH),PS3-cell)
# PROJECT_CPPFLAGS += -D_PS3 -DPS3_GAMESPY
#endif

# vim:ts=8:sw=8

