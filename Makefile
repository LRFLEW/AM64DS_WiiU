#-------------------------------------------------------------------------------
.SUFFIXES:
#-------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)

include $(DEVKITPRO)/wut/share/wut_rules

TARGET		:=	am64ds
BUILD		:=	build
SOURCES		:=	installer
PPC_ASM		:=	ppc_asm
ARM_ASM		:=	arm_asm
META		:=	meta

#-------------------------------------------------------------------------------
# options for code generation
#-------------------------------------------------------------------------------
DEBUG_LOG	=	0

CFLAGS		:=	-g -Wall -O2 -ffunction-sections -Wno-unused-value \
				$(MACHDEP)

CFLAGS		+=	$(INCLUDE) -D__WIIU__ -D__WUT__ -DDEBUG_LOG=$(DEBUG_LOG)

CXXFLAGS	:=	$(CFLAGS) -std=gnu++17

ASFLAGS		:=	-g $(ARCH)
LDFLAGS		=	-g $(ARCH) $(RPXSPECS) -Wl,-Map,$(notdir $*.map)

LIBS		:=	-lwut -lz

#-------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level
# containing include and lib
#-------------------------------------------------------------------------------
LIBDIRS	:=	$(PORTLIBS) $(WUT_ROOT)


#-------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#-------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#-------------------------------------------------------------------------------

export OUTPUT		:=	$(CURDIR)/$(TARGET)
export TOPDIR		:=	$(CURDIR)
export METADIR		:=	$(CURDIR)/$(META)
export DEPSDIR		:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
PPCSFILES	:=	$(foreach dir,$(PPC_ASM),$(notdir $(wildcard $(dir)/*.s)))
ARMSFILES	:=	$(foreach dir,$(ARM_ASM),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(PPCSFILES:.s=.bin) $(ARMSFILES:.s=.bin)
METAFILES	:=	$(foreach dir,$(META),$(wildcard $(dir)/*))

#-------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#-------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#-------------------------------------------------------------------------------
	export LD	:=	$(CC)
#-------------------------------------------------------------------------------
else
#-------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#-------------------------------------------------------------------------------
endif
#-------------------------------------------------------------------------------

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES_SRC	:=	$(CFILES:.c=.o) $(CPPFILES:.cpp=.o)
export OFILES 	:=	$(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN	:=	$(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE	:=	$(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean package all

#-------------------------------------------------------------------------------
all:	$(BUILD)

$(BUILD):
	$(SILENTCMD)[ -d $@ ] || mkdir -p $@
	$(SILENTCMD)VPATH=$(CURDIR)/$(ARM_ASM) $(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/$(ARM_ASM)/Makefile
	$(SILENTCMD)VPATH=$(CURDIR)/$(PPC_ASM) $(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/$(PPC_ASM)/Makefile
	$(SILENTCMD)VPATH=$(CURDIR)/$(SOURCES) $(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#-------------------------------------------------------------------------------
package:		$(BUILD) $(TARGET).zip
$(TARGET).zip:	$(TARGET).rpx $(METAFILES)
	$(SILENTMSG) package ...
	$(SILENTCMD)mkdir -p wiiu/apps/$(TARGET)
	$(SILENTCMD)cp $^ wiiu/apps/$(TARGET)/
	$(SILENTCMD)zip -q $@ $(foreach file,$?,wiiu/apps/$(TARGET)/$(notdir $(file)))

#-------------------------------------------------------------------------------
clean:
	$(SILENTMSG) clean ...
	$(SILENTCMD)rm -fr $(BUILD) $(TARGET).elf $(TARGET).rpx wiiu $(TARGET).zip

#-------------------------------------------------------------------------------
else
.PHONY:	all

DEPENDS	:=	$(OFILES:.o=.d)

#-------------------------------------------------------------------------------
# main targets
#-------------------------------------------------------------------------------
all	:	$(OUTPUT).rpx

$(OUTPUT).rpx	:	$(OUTPUT).elf
$(OUTPUT).elf	:	$(OFILES)

$(OFILES_SRC)	: $(HFILES_BIN)

#-------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#-------------------------------------------------------------------------------
%.bin.o	%_bin.h :	%.bin
#-------------------------------------------------------------------------------
	$(SILENTMSG) $(notdir $<)
	$(SILENTCMD)$(bin2o)

-include $(DEPENDS)

#-------------------------------------------------------------------------------
endif
#-------------------------------------------------------------------------------
