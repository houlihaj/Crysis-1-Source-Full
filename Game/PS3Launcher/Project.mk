#############################################################################
## Crytek Source File
## Copyright (C) 2007, Crytek Studios
##
## Creator: Sascha Demetrio
## Date: Apr 05 2007
## Description: GNU-make based build system
#############################################################################

PROJECT_TYPE := program

PROJECT_PS3_PRX := 1

PROJECT_CPPFLAGS_COMMON := \
	-I$(CODE_ROOT)/CryEngine/CryCommon \
	-I$(CODE_ROOT)/CryEngine/CrySystem \
	-I$(CODE_ROOT)/CryEngine/CryAction

# Local option enabling the use of the dmalloc library in the malloc wrapper
# code.  This will become a global option if this turns out to be useful.
# Note that $(option_use_dmalloc) is evaluated only if
# $(OPTION_PS3_MALLOCWRAPPER) is enabled.
option_use_dmalloc ?= 1

dmalloc_version := 5.5.2-1
dmalloc_dir := dmalloc-$(dmalloc_version)

ifeq ($(OPTION_PS3_MALLOCWRAPPER),1)
 ifeq ($(option_use_dmalloc),1)
  PROJECT_CPPFLAGS += -DUSE_DMALLOC -I$(PROJECT_CODE)/$(dmalloc_dir)
 endif
endif

# The list of modules (shared and/or static objects) that should be linked to
# the final executable.
ifeq ($(OPTION_PS3_PRX),1)
 PROJECT_LINKMODULES := CrySystem GameDll CryNetwork
 PROJECT_DEPMODULES := \
	$(PROJECT_LINKMODULES) \
	CryAction \
	Cry3DEngine \
	CryAISystem \
	CryAnimation \
	CryEntitySystem \
	CryFont \
	CryInput \
	CryMovie \
	CryPhysics \
	CryScriptSystem \
	CrySoundSystem \
	$(PROJECTS_Renderer)
else
 PROJECT_LINKMODULES := \
	CrySystem \
	GameDll \
	CryAction \
	Cry3DEngine \
	CryAISystem \
	CryAnimation \
	CryEntitySystem \
	CryFont \
	CryInput \
	CryMovie \
	CryNetwork \
	CryPhysics \
	CryScriptSystem \
	CrySoundSystem \
	$(PROJECTS_Renderer)
endif

include $(MAKE_ROOT)/Lib/ps3prxdefs.mk

PROJECT_SOURCES_CPP := Main.cpp
PROJECT_SOURCES_H := StdAfx.h
PROJECT_SOURCE_PCH := StdAfx.h

ifeq ($(OPTION_PROFILE),1)
 PROJECT_XFLAGS := -O3
endif

PROJECT_LDFLAGS := \
	-L$(CODE_ROOT)/Tools/PS3JobManager/lib\
	-L$(CODE_ROOT)/SDKs/Scaleform/Lib
ifeq ($(OPTION_PS3_GCMHUD),1)
 gcm_libs := -lgcm_hud -lgcm_pm 
else
 ifeq ($(OPTION_PS3_GCMDEBUG),1)
  gcm_libs := -lgcm_cmddbg -lgcm_sys_stub
 else
  gcm_libs := -lgcm_cmd -lgcm_sys_stub
 endif
endif
PROJECT_LDLIBS := \
	$(gcm_libs) \
	-lcgc \
	-lio_stub -lfs_stub -lrtc_stub \
	-lsysutil_stub -lsysmodule_stub -lnet_stub \
	-lpthread -lnetctl_stub -lsheap_stub -lcontrol_console_ppu \
	-lGFx_opt_nortti -ljpeg	-lm
ifeq ($(OPTION_PS3_PRX),1)
 PROJECT_LDLIBS += \
	$(CELL_SDK)/target/ppu/lib/fno-exceptions/fno-rtti/libc_libent.o \
	$(CELL_SDK)/target/ppu/lib/fno-exceptions/fno-rtti/libm_libent.o \
	$(CELL_SDK)/target/ppu/lib/fno-exceptions/fno-rtti/libstdc++_libent.o
endif

ifeq ($(OPTION_PS3_MALLOCWRAPPER),1)

PROJECT_OBJECTS_ADD = $(PROJECT_BUILD)/Malloc.o

MALLOCWRAPPER_CPPFLAGS =
MALLOCWRAPPER_LDFLAGS =
MALLOCWRAPPER_LIBS =
malloc_from_libc := 1
ifeq ($(option_use_dmalloc),1)
 MALLOCWRAPPER_CPPFLAGS += \
 	-DMALLOCWRAPPER_USE_DMALLOC \
	-I$(PROJECT_CODE)/$(dmalloc_dir)
 MALLOCWRAPPER_LDFLAGS += -L$(PROJECT_CODE)/$(dmalloc_dir)
 MALLOCWRAPPER_LIBS += -ldmallocth
 malloc_from_libc := 0
endif
ifeq ($(malloc_from_libc),1)
 MALLOCWRAPPER_LIBS += -lc
endif

##############################################################################
# Special rules for buiding the mallocwrapper object Malloc.o.

MALLOCWRAPPER_OBJECTS = \
	$(PROJECT_BUILD)/MallocWrapper.o \
	$(PROJECT_BUILD)/RealMalloc.o
ifneq ($(malloc_from_libc),1)
 MALLOCWRAPPER_OBJECTS += $(PROJECT_BUILD)/LibcMalloc.o
endif

# Symbols exported by the malloc wrapper.
MALLOCWRAPPER_SYMBOLS := \
	malloc calloc realloc free memalign reallocalign valloc strdup

MALLOCWRAPPER_LIBC := $(CELL_SDK)/target/ppu/lib/fno-exceptions/fno-rtti/libc.a 

MALLOCWRAPPER_LIBREALMALLOC_OBJECTS = \
	$(PROJECT_BUILD)/memcpy.o

$(PROJECT_BUILD)/libRealMalloc.a: $(MALLOCWRAPPER_LIBREALMALLOC_OBJECTS)
	$(SILENT) rm -f $@
	$(BUILD_LINK) $(AR) cq $@ $^
	$(BUILD_LINK) $(RANLIB) $@

# LIBC symbols which should _not_ be renamed when linking RealMalloc.o.  These
# are the exported names from libc.a(malloc.o) which are _not_ wrapped by the
# malloc wrapper.
$(PROJECT_BUILD)/mallocwrapper_symbols.mk:
	$(BUILD_ECHO) 'Scanning libc.a(malloc.o) for exported symbols'
	$(SILENT) rm -f $@
	$(BUILD_LINK) \
		$(NM) $(MALLOCWRAPPER_LIBC) 2>/dev/null \
		|sed -ne '/^malloc.o:$$/,/^\s*$$/p' \
		|sed -ne 's/^[0-9a-f]*\s\+[DT]\s\+//p' \
		$(foreach symbol,\
			$(MALLOCWRAPPER_SYMBOLS) memcpy,\
			|grep -wv '$(symbol)') \
		|(echo -n 'MALLOCWRAPPER_LIBCMALLOCSYMS := '; xargs echo) \
		>$@

include $(PROJECT_BUILD)/mallocwrapper_symbols.mk

# Temporary hack until all files have reached their final destination.
#_mwdir := MallocWrapper
_mwdir :=

$(PROJECT_BUILD)/Malloc.o: $(MALLOCWRAPPER_OBJECTS)
	$(BUILD_ECHO) 'Linking Malloc.o (MallocWrapper)'
	$(BUILD_LINK) $(LD) -r $(LDRFLAGS) -o $@ $^

$(PROJECT_BUILD)/RealMalloc.o: \
  $(PROJECT_CODE)/$(_mwdir)/RealMalloc.c \
  $(PROJECT_BUILD)/libRealMalloc.a
	$(BUILD_ECHO) 'RealMalloc.c (MallocWrapper)'
	$(SILENT) mkdir -p $(PROJECT_BUILD)/tmp
	$(BUILD) $(CC) $(MALLOCWRAPPER_CPPFLAGS) $(CPPFLAGS) $(CFLAGS) \
		-c -g -o $(PROJECT_BUILD)/tmp/RealMalloc_1.o $<
	$(BUILD_ECHO) 'Linking malloc symbols (MallocWrapper)'
	$(BUILD_LINK) $(LD) -g -r $(LDRFLAGS) \
		-o $(PROJECT_BUILD)/tmp/RealMalloc_2.o \
		$(PROJECT_BUILD)/tmp/RealMalloc_1.o \
		-L $(CELL_SDK)/target/ppu/lib \
		$(MALLOCWRAPPER_LDFLAGS) $(MALLOCWRAPPER_LIBS) \
		-L $(PROJECT_BUILD) -lRealMalloc
	$(SILENT) rm -f $(PROJECT_BUILD)/tmp/RealMalloc_1.o
	$(BUILD_ECHO) 'Redefining malloc symbols (MallocWrapper)'
	$(BUILD_LINK) $(NM) $(PROJECT_BUILD)/tmp/RealMalloc_2.o \
		|grep -v mallocwrapper_ \
		|grep -v dmalloc_ \
		$(foreach symbol,\
			$(MALLOCWRAPPER_LIBCMALLOCSYMS),\
			|grep -wv '$(symbol)') \
		|sed -ne 's/^[0-9a-f]*\s\+[BDTR]\s\+//p' \
		>$(PROJECT_BUILD)/tmp/RealMalloc_libc.syms
	$(BUILD_LINK) $(OBJCOPY) \
		`sed \
		  -e 's/^\.\(\S*\)\$$/--redefine-sym .\1=.mwprivate1_\1/' \
		  -e 's/^[^\. ]\S*\$$/--redefine-sym \0=mwprivate1_\0/' \
		  $(PROJECT_BUILD)/tmp/RealMalloc_libc.syms` \
		$(PROJECT_BUILD)/tmp/RealMalloc_2.o \
		$@
	$(SILENT) rm -f $(PROJECT_BUILD)/tmp/RealMalloc_2.o
	$(SILENT) rm -f $(PROJECT_BUILD)/tmp/RealMalloc_libc.syms

$(PROJECT_BUILD)/LibcMalloc.o: $(MALLOCWRAPPER_LIBC)
	$(SILENT) mkdir -p $(PROJECT_BUILD)/tmp
	$(SILENT) rm -f $(PROJECT_BUILD)/tmp/malloc.o
	$(SILENT) rm -f $(PROJECT_BUILD)/tmp/libc_malloc.o
	$(SILENT) (cd $(PROJECT_BUILD)/tmp \
		&& $(_BUILD_LINK) $(AR) x $(MALLOCWRAPPER_LIBC) malloc.o \
		&& mv malloc.o libc_malloc.o)
	$(BUILD_LINK) $(OBJCOPY) \
		$(foreach symbol,\
			$(MALLOCWRAPPER_SYMBOLS),\
			--redefine-sym $(symbol)=mwprivate2_$(symbol) \
			--redefine-sym .$(symbol)=.mwprivate2_$(symbol)) \
		--redefine-sym memcpy=mwprivate1_memcpy \
		--redefine-sym .memcpy=.mwprivate1_memcpy \
		$(PROJECT_BUILD)/tmp/libc_malloc.o \
		$@
	$(SILENT) rm -f $(PROJECT_BUILD)/tmp/libc_malloc.o

$(PROJECT_BUILD)/MallocWrapper.o: $(PROJECT_CODE)/$(_mwdir)/MallocWrapper.c
	$(BUILD_ECHO) 'MallocWrapper.c (MallocWrapper)'
	$(BUILD) $(CC) $(CPPFLAGS) $(CFLAGS) -c -g -o $@ $<

$(PROJECT_BUILD)/%.o: $(PROJECT_CODE)/MallocWrapper/%.c
	$(BUILD) $(CC) $(CPPFLAGS) $(CFLAGS) -c -g -o $@ $<

endif # OPTION_PS3_MALLOCWRAPPER == 1

# vim:ts=8:sw=8

