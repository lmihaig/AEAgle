################################################################################
# Makefile (fixed): Run SysConfig once, then ALWAYS patch “heap_4.c → heap_<n>.c”
# right before compiling ti_freertos_config.c, so that manual edits are never
# overwritten by SysConfig.
################################################################################

#------------------------------------------------------------------------------
# 1) Paths and Variables
#------------------------------------------------------------------------------
SDK_DIR      := /home/lmg/ti/simplelink_cc13xx_cc26xx_sdk_8_30_01_01
SYS_CFG      := /home/lmg/ti/sysconfig_1.21.1/sysconfig_cli.sh
BUILD_DIR    := build

# Use GCC 9.3.1 from the TI toolchain
CC           := /home/lmg/ti/gcc-arm-none-eabi_9_3_1/bin/arm-none-eabi-gcc
OBJCOPY      := /home/lmg/ti/gcc-arm-none-eabi_9_3_1/bin/arm-none-eabi-objcopy

# User‐selectable heap implementation: 1, 2, 3, 4, or 5.
# Defaults to 4 (heap_4.c) if HEAP_IMPL is not set externally.
HEAP_IMPL    ?= 4

#------------------------------------------------------------------------------
# 2) Output Filenames
#------------------------------------------------------------------------------
ELF          := $(BUILD_DIR)/demo-freertos.elf
HEX          := $(BUILD_DIR)/demo-freertos.hex
MAP          := $(BUILD_DIR)/demo-freertos.map

#------------------------------------------------------------------------------
# 3) Compiler & Linker Flags
#------------------------------------------------------------------------------
CPUFLAGS     := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
DEFS         := -DDeviceFamily_CC13X2
CFLAGS       := $(CPUFLAGS) -Os -g3 -ffunction-sections -fdata-sections -std=c11 $(DEFS)

LDFLAGS      := \
  -T$(CURDIR)/cc13x2_cc26x2_freertos.lds \
  -nostartfiles -static -Wl,--gc-sections -Wl,-Map,$(MAP) \
  -L$(BUILD_DIR) \
  -L$(SDK_DIR)/source/ti/drivers/lib/gcc/m4f \
  -L$(SDK_DIR)/source/ti/devices/cc13x2_cc26x2/driverlib/bin/gcc \
  -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard --specs=nano.specs

DRIVERS_LIB  := $(SDK_DIR)/source/ti/drivers/lib/gcc/m4f/drivers_cc13x2.a
DRIVERLIB    := $(SDK_DIR)/source/ti/devices/cc13x2_cc26x2/driverlib/bin/gcc/driverlib.lib

LIBS         := \
  $(DRIVERS_LIB) \
  $(DRIVERLIB) \
  -lc -lm -lnosys

#------------------------------------------------------------------------------
# 4) Include Paths (match Code Composer Studio’s defaults)
#------------------------------------------------------------------------------
INCLUDES     := \
  -I$(SDK_DIR)/source \
  -I$(SDK_DIR)/source/third_party/freertos/include \
  -I$(SDK_DIR)/source/third_party/freertos/portable/GCC/ARM_CM4F \
  -I$(SDK_DIR)/source/third_party/freertos/portable/MemMang \
  -I$(SDK_DIR)/kernel/freertos \
  -I$(SDK_DIR)/kernel/freertos/dpl \
  -I$(SDK_DIR)/source/ti/posix/gcc \
  -I$(SDK_DIR)/source/ti/drivers \
  -I$(SDK_DIR)/source/ti/devices/cc13x2_cc26x2 \
  -I$(BUILD_DIR) \
  -I$(CURDIR)

#------------------------------------------------------------------------------
# 5) Source Files and Object‐File Lists
#------------------------------------------------------------------------------
APP_SRC      := main.c
SYS_SRCS     := \
  $(BUILD_DIR)/ti_drivers_config.c \
  $(BUILD_DIR)/ti_devices_config.c \
  $(BUILD_DIR)/ti_freertos_config.c \
  $(BUILD_DIR)/ti_freertos_portable_config.c

ALL_SRCS     := $(APP_SRC) $(notdir $(SYS_SRCS))
OBJS         := $(addprefix $(BUILD_DIR)/,$(ALL_SRCS:.c=.o))

# Let Make find .c files in both the current directory and the build directory
VPATH        := .:$(BUILD_DIR)

# 6) Default target
.PHONY: all clean FORCE
all: $(HEX)

# 7) Ensure build directory
$(BUILD_DIR):
	mkdir -p $@

# 8) Step 1: Run SysConfig CLI if needed
$(BUILD_DIR)/ti_freertos_config.c: demo-freertos.syscfg | $(BUILD_DIR)
	@echo "=== [1/2] Running SysConfig (heap=$(HEAP_IMPL)) ==="
	$(SYS_CFG) --script $< --compiler gcc \
	  -s $(SDK_DIR)/.metadata/product.json \
	  --output $(BUILD_DIR)

# 9) Step 2: ALWAYS patch out any heap_[0-9].c → heap_$(HEAP_IMPL).c
$(BUILD_DIR)/ti_freertos_config.c: FORCE
	@echo "=== [2/2] Patching any heap_X.c → heap_$(HEAP_IMPL).c ==="
	@sed -i \
	  's@\(#include[[:space:]]*<\)\([^>]*heap_[0-9]\+\.c\)\(>\)@\1../portable/MemMang/heap_$(HEAP_IMPL).c\3@' \
	  $(BUILD_DIR)/ti_freertos_config.c || true

.PHONY: FORCE
FORCE:

# 9b) Make other SysConfig‐generated stubs depend on the patched ti_freertos_config.c
$(BUILD_DIR)/ti_drivers_config.c:       $(BUILD_DIR)/ti_freertos_config.c
$(BUILD_DIR)/ti_devices_config.c:       $(BUILD_DIR)/ti_freertos_config.c
$(BUILD_DIR)/ti_freertos_portable_config.c: $(BUILD_DIR)/ti_freertos_config.c
$(BUILD_DIR)/FreeRTOSConfig.h:          $(BUILD_DIR)/ti_freertos_config.c

# 10) Compile all sources → object files (they depend on the patched config)
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR) $(BUILD_DIR)/ti_freertos_config.c
	@echo "Compiling $< → $@"
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# 11) Link: .o → ELF
$(ELF): $(OBJS) | $(BUILD_DIR)
	@echo "Linking into $(ELF)…"
	$(CC) $(CPUFLAGS) $(OBJS) $(LIBS) $(LDFLAGS) -o $@

# 12) Generate HEX from ELF
$(HEX): $(ELF)
	@echo "Generating $(HEX)…"
	$(OBJCOPY) -O ihex $< $@

# 13) Clean
clean:
	@echo "Cleaning $(BUILD_DIR)…"
	rm -rf $(BUILD_DIR)
