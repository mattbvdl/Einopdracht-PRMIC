# MCU and project
MCU := atxmega256a3u
TARGET ?= $(notdir $(CURDIR))
BUILD := build

# Tools
CC := avr-gcc
CXX := avr-g++
OBJCOPY := avr-objcopy

# Compiler and linker flags
CFLAGS := -mmcu=$(MCU)  -Os -std=gnu11 -Iinclude -D__DELAY_BACKWARD_COMPATIBLE__
CFLAGS += -ffunction-sections
CFLAGS += -fdata-sections
CXXFLAGS := -mmcu=$(MCU) -Os -std=gnu++17 -Iinclude -D__DELAY_BACKWARD_COMPATIBLE__
CXXFLAGS += -ffunction-sections
CXXFLAGS += -fdata-sections
LDFLAGS := -mmcu=$(MCU)
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,-u,vfprintf
LDFLAGS += -Wl,-Map=$(BUILD)/$(TARGET).map
LDLIBS := -lprintf_flt

# Source and object files
C_SRC := $(shell find . -name '*.c')
CPP_SRC := $(shell find . -name '*.cpp')
OBJ := $(patsubst %.c,$(BUILD)/%.o,$(C_SRC)) \
       $(patsubst %.cpp,$(BUILD)/%.o,$(CPP_SRC))

# Output files
HEX := $(BUILD)/$(TARGET).hex
ELF := $(BUILD)/$(TARGET).elf

# Default target
.PHONY: all flash clean
all: $(HEX)

# Build directory
$(BUILD):
	@mkdir -p $@

# Link
$(ELF): $(OBJ)
	@$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Create HEX file
$(HEX): $(ELF)
	@$(OBJCOPY) -O ihex $< $@

# Compile C
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

# Compile C++
$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# Flash
flash: $(HEX)
	@avrdude -e -p x256a3u -c avrispmkII -P usb -U flash:w:$<
	@rm -rf $(BUILD)
	
# Check for source files
ifeq ($(strip $(C_SRC)$(CPP_SRC)),)
$(error No .c or .cpp files found)
endif