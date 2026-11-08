# Makefile - builds GSEngine for the Game Boy Advance with devkitARM.
#
# Needs the DEVKITARM variable (the devkitpro/devkitarm Docker image sets it).
# Put the engine in GSCore/ and the platform layer in gba/.

ifeq ($(strip $(DEVKITARM)),)
$(error DEVKITARM is not set. Install devkitPro or use the devkitpro/devkitarm image)
endif

TARGET   := GSEngine
BUILD    := build
SRC_DIRS := GSCore gba

PREFIX   := $(DEVKITARM)/bin/arm-none-eabi-
CC       := $(PREFIX)gcc
OBJCOPY  := $(PREFIX)objcopy
GBAFIX   := $(DEVKITPRO)/tools/bin/gbafix

ARCH     := -mthumb -mthumb-interwork -mcpu=arm7tdmi -mtune=arm7tdmi
CFLAGS   := -std=gnu99 -O2 -Wall -Wextra $(ARCH) \
            -ffunction-sections -fdata-sections \
            $(addprefix -I,$(SRC_DIRS))
LDFLAGS  := -specs=gba.specs $(ARCH) -Wl,--gc-sections

SOURCES  := $(foreach d,$(SRC_DIRS),$(wildcard $(d)/*.c))
OBJECTS  := $(patsubst %.c,$(BUILD)/%.o,$(SOURCES))

all: $(TARGET).gba

$(TARGET).gba: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	@if [ -x "$(GBAFIX)" ]; then $(GBAFIX) $@ -t$(TARGET); \
	else echo "note: gbafix not found, header not fixed (fine for emulators)"; fi
	@echo "built $@"

$(TARGET).elf: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).gba

.PHONY: all clean
