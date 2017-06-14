# Target architecture for the build. Use msp430 if you are unsure.
export TARGET_ARCH ?= msp430-elf

# define build specific options
CFLAGS_CPU   = -mmcu=$(CPU_MODEL) -std=gnu99
CFLAGS_LINK  = -ffunction-sections -fdata-sections
CFLAGS_DBG  ?= -gdwarf-2
CFLAGS_OPT  ?= -Os
# export compiler flags
export CFLAGS += $(CFLAGS_CPU) $(CFLAGS_LINK) $(CFLAGS_DBG) $(CFLAGS_OPT)
# export assmebly flags
export ASFLAGS += $(CFLAGS_CPU) --defsym $(CPU_MODEL)=1 $(CFLAGS_DBG)
# export linker flags
export LINKFLAGS += $(CFLAGS_CPU) $(CFLAGS_DBG) $(CFLAGS_OPT) -L/usr/msp430-elf/lib -T$(CPU_MODEL).ld -Wl,--gc-sections -static -lgcc

#export LINKFLAGS += -Wl,--defsym=_eheap=0 -Wl,--defsym=_sheap=0
