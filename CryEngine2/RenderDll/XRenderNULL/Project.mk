#############################################################################
## Crytek Source File
## Copyright (C) 2006, Crytek Studios
##
## Creator: Sascha Demetrio
## Date: Jul 31, 2006
## Description: GNU-make based build system
#############################################################################

PROJECT_TYPE := module
PROJECT_VCPROJ := XRenderNULL.vcproj

PROJECT_CPPFLAGS_COMMON := \
	-I$(CODE_ROOT)/CryEngine/CryCommon \
	-I$(PROJECT_CODE)/.. \
	-I$(PROJECT_CODE)/../Common/Textures/Image/NVDXT \
	-I$(PROJECT_BUILD)/.. \
	-DNULL_RENDERER

PROJECT_SOURCES_CPP_REMOVE := ../RenderPCH.cpp

ifneq ($(ARCH),PS3-cell)
 PROJECT_SOURCES_C_ADD := $(PROJECT_CODE)/../Common/Textures/Image/Jmemsrc.c
endif

# vim:ts=8:sw=8

