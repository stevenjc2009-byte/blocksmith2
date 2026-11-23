#---------------------------------------------------------------------------------
# Blocksmith 2 build.
#
#   make          PC tests, then blocksmith2.3dsx and blocksmith2.cia
#   make test     PC tests only (host gcc, no 3DS tools)
#   make 3dsx     PC tests, then the .3dsx
#   make clean    remove everything the build made
#
# Based on the devkitPro 3DS application template
# (C:/devkitPro/examples/3ds/templates/application/Makefile), with PC tests and
# a .cia step added.
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# Game details
#---------------------------------------------------------------------------------
TARGET		:=	blocksmith2
BUILD		:=	build
SOURCES		:=	source
DATA		:=	data
INCLUDES	:=	include
GRAPHICS	:=	gfx
GFXBUILD	:=	$(BUILD)

APP_TITLE	:=	Blocksmith 2
APP_DESCRIPTION	:=	Build, mine and explore
APP_AUTHOR	:=	Homebrew
ICON		:=	meta/icon.png

# Version (kickoff answer K-7: start at 0.1).
VERSION_MAJOR	:=	0
VERSION_MINOR	:=	1
VERSION_MICRO	:=	0

#---------------------------------------------------------------------------------
# PC tests: pure C files that build with the host compiler, plus tests/*.c
#---------------------------------------------------------------------------------
HOSTCC		?=	gcc
HOST_BUILD	:=	build-host
CORE_SOURCES	:=	source/world_coords.c source/framebuffer.c source/mat4.c \
			source/texswizzle.c source/blocktex.c source/block.c source/mesh.c \
			source/look.c source/camera.c source/chunk.c source/chunk_mesh.c source/terrain.c source/world.c \
			source/walk.c source/player.c source/raycast.c source/place.c source/hotbar.c \
			source/font.c source/fps.c source/stream.c source/settings.c \
			source/seed.c source/newworld.c source/save.c source/save_files.c source/saveui.c \
			source/worldname.c source/save_worlds.c source/worldlist.c source/lidsave.c \
			source/save_backup.c
TEST_SOURCES	:=	$(wildcard tests/*.c)

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS	:=	-g -Wall -O2 -mword-relocations \
			-ffunction-sections \
			$(ARCH)

CFLAGS	+=	$(INCLUDE) -D__3DS__

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:= -lctru -lm

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:= $(CTRULIB)


#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(GRAPHICS),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
PICAFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.v.pica)))
SHLISTFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.shlist)))
GFXFILES	:=	$(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.t3s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

#---------------------------------------------------------------------------------
ifeq ($(GFXBUILD),$(BUILD))
export T3XFILES :=  $(GFXFILES:.t3s=.t3x)
else
export ROMFS_T3XFILES	:=	$(patsubst %.t3s, $(GFXBUILD)/%.t3x, $(GFXFILES))
export T3XHFILES		:=	$(patsubst %.t3s, $(BUILD)/%.h, $(GFXFILES))
endif

export OFILES_SOURCES 	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES)) \
			$(PICAFILES:.v.pica=.shbin.o) $(SHLISTFILES:.shlist=.shbin.o) \
			$(addsuffix .o,$(T3XFILES))

export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES	:=	$(PICAFILES:.v.pica=_shbin.h) $(SHLISTFILES:.shlist=_shbin.h) \
			$(addsuffix .h,$(subst .,_,$(BINFILES))) \
			$(GFXFILES:.t3s=.h)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export _3DSXDEPS	:=	$(OUTPUT).smdh

export APP_ICON := $(TOPDIR)/$(ICON)

export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh

.PHONY: all test 3dsx cia art clean

#---------------------------------------------------------------------------------
all: cia

#---------------------------------------------------------------------------------
# PC tests. The run fails the build if any check fails or none ran.
#---------------------------------------------------------------------------------
test: $(HOST_BUILD)/tests.exe
	@$(HOST_BUILD)/tests.exe

$(HOST_BUILD)/tests.exe: $(TEST_SOURCES) $(CORE_SOURCES) $(wildcard tests/*.h include/*.h)
	@mkdir -p $(HOST_BUILD)
	@echo host gcc: tests
	@$(HOSTCC) -std=c11 -O1 -Wall -Wextra -Werror -Iinclude -Itests \
		$(TEST_SOURCES) $(CORE_SOURCES) -o $@

#---------------------------------------------------------------------------------
# Art drawn in code (kickoff answer F-4): icon, Home Menu banner, banner sound.
#---------------------------------------------------------------------------------
ART	:=	meta/icon.png meta/banner.png meta/banner.wav

art: $(ART)

$(ART) &: tools/make_art.py
	@echo art: $^
	@py -3 tools/make_art.py meta

#---------------------------------------------------------------------------------
3dsx: test $(ART) $(BUILD) $(GFXBUILD) $(DEPSDIR) $(ROMFS_T3XFILES) $(T3XHFILES)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
# .cia for the Home Menu (kickoff answer T-8).
#---------------------------------------------------------------------------------
cia: 3dsx
	@echo bannertool: banner and icon
	@bannertool makebanner -i meta/banner.png -a meta/banner.wav -o $(BUILD)/banner.bnr > /dev/null
	@bannertool makesmdh -s "$(APP_TITLE)" -l "$(APP_DESCRIPTION)" -p "$(APP_AUTHOR)" \
		-i meta/icon.png -o $(BUILD)/icon.icn > /dev/null
	@echo makerom: $(TARGET).cia
	@makerom -f cia -o $(TARGET).cia -elf $(TARGET).elf -rsf meta/$(TARGET).rsf \
		-icon $(BUILD)/icon.icn -banner $(BUILD)/banner.bnr -exefslogo -target t \
		-major $(VERSION_MAJOR) -minor $(VERSION_MINOR) -micro $(VERSION_MICRO)

$(BUILD):
	@mkdir -p $@

ifneq ($(GFXBUILD),$(BUILD))
$(GFXBUILD):
	@mkdir -p $@
endif

ifneq ($(DEPSDIR),$(BUILD))
$(DEPSDIR):
	@mkdir -p $@
endif

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(HOST_BUILD) $(TARGET).3dsx $(OUTPUT).smdh $(TARGET).elf $(TARGET).cia $(GFXBUILD)

#---------------------------------------------------------------------------------
$(GFXBUILD)/%.t3x	$(BUILD)/%.h	:	%.t3s
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@tex3ds -i $< -H $(BUILD)/$*.h -d $(DEPSDIR)/$*.d -o $(GFXBUILD)/$*.t3x

#---------------------------------------------------------------------------------
else

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).3dsx	:	$(OUTPUT).elf $(_3DSXDEPS)

$(OFILES_SOURCES) : $(HFILES)

$(OUTPUT).elf	:	$(OFILES)

#---------------------------------------------------------------------------------
%.bin.o	%_bin.h :	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

#---------------------------------------------------------------------------------
.PRECIOUS	:	%.t3x %.shbin
#---------------------------------------------------------------------------------
%.t3x.o	%_t3x.h :	%.t3x
#---------------------------------------------------------------------------------
	$(SILENTMSG) $(notdir $<)
	$(bin2o)

#---------------------------------------------------------------------------------
%.shbin.o %_shbin.h : %.shbin
#---------------------------------------------------------------------------------
	$(SILENTMSG) $(notdir $<)
	$(bin2o)

-include $(DEPSDIR)/*.d

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
