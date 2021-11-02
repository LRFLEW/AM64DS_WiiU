#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPRO")
endif
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>DEVKITARM")
endif
ifeq ($(strip $(TOPDIR)),)
$(error "Don't call this make file directly. Call make on the parent directory.")
endif

include $(DEVKITARM)/base_rules

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ASFLAGS	:=

#---------------------------------------------------------------------------------
SFILES		:=	$(foreach dir,$(TOPDIR)/arm_asm,$(notdir $(wildcard $(dir)/*.s)))
OFILES		:=	$(SFILES:.s=.o)
BINFILES	:=	$(SFILES:.s=.bin)

.PHONY:	default
default:	all
	@:

all:	$(BINFILES)
.SECONDARY:	$(OFILES)

%.bin:	%.o
	$(SILENTMSG) $(notdir $<)
	$(SILENTCMD)$(OBJCOPY) -O binary $< $@
