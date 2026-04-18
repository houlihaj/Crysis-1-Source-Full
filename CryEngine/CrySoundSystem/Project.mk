#############################################################################
## Crytek Source File
## Copyright (C) 2006, Crytek Studios
##
## Creator: Sascha Demetrio
## Date: Jul 31, 2006
## Description: GNU-make based build system
#############################################################################

PROJECT_TYPE := module
PROJECT_VCPROJ := CrySoundSystem.vcproj

PROJECT_PS3_PRX := 1
ifeq ($(ARCH),PS3-cell)
 PROJECT_STUB_SOURCES_CPP := CrySoundSystem.cpp
 PROJECT_LDLIBS := -lpthread -lm_stub -lfs_stub -lnet_stub
endif

PROJECT_LINKMODULES := CrySystem

PROJECT_CPPFLAGS_COMMON := \
	-I$(CODE_ROOT)/CryEngine/CryCommon

PROJECT_SOURCES_CPP_REMOVE := StdAfx.cpp

#ifeq ($(ARCH),PS3-cell)
#PROJECT_OBJECTS_ADD := $(PROJECT_BUILD)/FmodExSPURS.o
#
#$(PROJECT_BUILD)/FmodExSPURS.o: $(PROJECT_BUILD)/FmodExSPURS.c
#	$(SILENT) $(RM) '$@'
#	$(BUILD_ECHO) $(tag_c) $($(TARGET)_CODE)/$(notdir $<)
#	$(BUILD) $(CC) -c -o '$@' $^

fmodex_libdir := $(PROJECT_CODE)/FmodEx/lib/ps3

#$(PROJECT_BUILD)/FmodExSPURS.c: \
#  $(fmodex_libdir)/fmodex_spurs.elf \
#  $(fmodex_libdir)/fmodex_spurs_mpeg.elf
#	$(SILENT) $(RM) '$@'
#	$(BUILD_ECHO) Generating $($(TARGET)_CODE)/$(notdir $@)
#	$(BUILD_SILENT) $(PERL) '$(MAKE_ROOT)/Tools/bin2c.pl' \
#	  _binary_fmodex_spurs_elf_start \
#	  $(fmodex_libdir)/fmodex_spurs.elf \
#	  >>'$@'
#	$(BUILD_SILENT) $(PERL) '$(MAKE_ROOT)/Tools/bin2c.pl' \
#	  _binary_fmodex_spurs_mpeg_elf_start \
#	  $(fmodex_libdir)/fmodex_spurs_mpeg.elf \
#	  >>'$@'
#
#COMPILE_CLEAN_$(TARGET) += \
#	$(PROJECT_BUILD)/FmodExSPURS.o \
#	$(PROJECT_BUILD)/FmodExSPURS.c
#
#endif

# vim:ts=8:sw=8

