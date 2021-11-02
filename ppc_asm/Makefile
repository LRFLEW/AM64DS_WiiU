#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPRO")
endif
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif
ifeq ($(strip $(TOPDIR)),)
$(error "Don't call this make file directly. Call make on the parent directory.")
endif

include $(DEVKITPPC)/base_rules

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ASFLAGS	:=	-mregnames

#---------------------------------------------------------------------------------
SFILES		:=	$(foreach dir,$(TOPDIR)/ppc_asm,$(notdir $(wildcard $(dir)/*.s)))
OFILES		:=	$(SFILES:.s=.o)
BINFILES	:=	$(SFILES:.s=.bin)
HFILES		:=	$(addsuffix .h,$(subst .,_,$(SFILES)))


.PHONY:	default
default: all
	@:

all:	$(BINFILES) $(HFILES)
.SECONDARY:	$(OFILES)

%.bin:	%.o
	$(SILENTMSG) $(notdir $<)
	$(SILENTCMD)$(OBJCOPY) -O binary $< $@

%_s.h:	%.o
	$(SILENTMSG) $(notdir $<).meta
	$(SILENTCMD)$(NM) -g $< | sed -E 's/^([0-9A-Fa-f]{8}) T (inject_.*)$$/static constexpr size_t off_\2 = 0x\1;/' > $@
