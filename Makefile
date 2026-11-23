PROJECT := opendws200
VERSION ?= v0.2.6-phase2
BUILD := build
DIST := dist
OUT_BIN := $(DIST)/APP_DWS_200_OpenDWS-$(VERSION).bin

CLANG ?= clang
OBJCOPY ?= llvm-objcopy
OBJDUMP ?= llvm-objdump
PYTHON ?= python3
NODE ?= node

CFLAGS := \
	-target armv7em-none-eabi \
	-mcpu=cortex-m4 \
	-mfloat-abi=soft \
	-mthumb \
	-ffreestanding \
	-fno-builtin \
	-fdata-sections \
	-ffunction-sections \
	-fno-unwind-tables \
	-fno-asynchronous-unwind-tables \
	-Os \
	-Wall \
	-Wextra \
	-Werror \
	-Isrc \
	-DOPENDWS_VERSION_STR='"$(VERSION)"'

LDFLAGS := \
	-target armv7em-none-eabi \
	-mcpu=cortex-m4 \
	-mfloat-abi=soft \
	-mthumb \
	-nostdlib \
	-fuse-ld=lld \
	-Wl,-T,linker.ld \
	-Wl,--gc-sections \
	-Wl,-Map,$(BUILD)/$(PROJECT).map

SRCS := \
	src/startup.c \
	src/runtime.c \
	src/fmt.c \
	src/board.c \
	src/input.c \
	src/sensors.c \
	src/heater.c \
	src/settings.c \
	src/menu.c \
	src/display.c \
	src/font.c \
	src/gfx.c \
	src/ui.c \
	src/protocol.c \
	src/usb_cdc.c \
	src/main.c

OBJS := $(patsubst src/%.c,$(BUILD)/arm/%.o,$(SRCS))

HOST_FLAGS := -std=c11 -O2 -Wall -Wextra -Werror -DOPENDWS_HOST_TEST=1 -Isrc -fsanitize=address,undefined
HOST_TESTS := \
	$(BUILD)/tests/test_sensors \
	$(BUILD)/tests/test_heater \
	$(BUILD)/tests/test_input \
	$(BUILD)/tests/test_menu \
	$(BUILD)/tests/test_protocol \
	$(BUILD)/tests/test_settings \
	$(BUILD)/tests/test_ui

.PHONY: all firmware test preview screenshots verify-bin clean

all: firmware

firmware: $(OUT_BIN)

$(BUILD)/arm/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(BUILD)/$(PROJECT).elf: $(OBJS) linker.ld
	@mkdir -p $(dir $@)
	$(CLANG) $(OBJS) $(LDFLAGS) -o $@

$(OUT_BIN): $(BUILD)/$(PROJECT).elf
	@mkdir -p $(DIST)
	$(OBJCOPY) -O binary $< $@
	sha256sum $@ > $@.sha256

verify-bin: $(OUT_BIN) $(BUILD)/$(PROJECT).elf
	$(OBJDUMP) -d $(BUILD)/$(PROJECT).elf > $(BUILD)/$(PROJECT).disasm
	$(NODE) tools/verify_bin.js $(OUT_BIN) --disasm $(BUILD)/$(PROJECT).disasm

$(BUILD)/tests/test_sensors: tests/test_sensors.c src/sensors.c src/sensors.h
	@mkdir -p $(dir $@)
	$(CLANG) $(HOST_FLAGS) $< src/sensors.c -o $@

$(BUILD)/tests/test_heater: tests/test_heater.c src/heater.c src/heater.h src/sensors.c src/sensors.h src/board.c
	@mkdir -p $(dir $@)
	$(CLANG) $(HOST_FLAGS) $< src/heater.c src/sensors.c src/board.c -o $@

$(BUILD)/tests/test_input: tests/test_input.c src/input.c src/input.h
	@mkdir -p $(dir $@)
	$(CLANG) $(HOST_FLAGS) $< src/input.c -o $@

$(BUILD)/tests/test_menu: tests/test_menu.c src/menu.c src/menu.h src/settings.c src/fmt.c
	@mkdir -p $(dir $@)
	$(CLANG) $(HOST_FLAGS) $< src/menu.c src/settings.c src/fmt.c -o $@

$(BUILD)/tests/test_protocol: tests/test_protocol.c src/protocol.c src/protocol.h src/heater.c src/sensors.c src/board.c src/fmt.c
	@mkdir -p $(dir $@)
	$(CLANG) $(HOST_FLAGS) -DOPENDWS_VERSION_STR='"$(VERSION)"' $< src/protocol.c src/heater.c src/sensors.c src/board.c src/fmt.c -o $@

$(BUILD)/tests/test_settings: tests/test_settings.c src/settings.c src/settings.h
	@mkdir -p $(dir $@)
	$(CLANG) $(HOST_FLAGS) $< src/settings.c -o $@

$(BUILD)/tests/test_ui: tests/test_ui.c tools/host_display.h src/display.c src/font.c src/gfx.c src/fmt.c src/ui.c src/menu.c src/heater.c src/sensors.c src/settings.c src/board.c
	@mkdir -p $(dir $@)
	$(CLANG) $(HOST_FLAGS) $< src/display.c src/font.c src/gfx.c src/fmt.c src/ui.c src/menu.c src/heater.c src/sensors.c src/settings.c src/board.c -o $@

test: $(HOST_TESTS)
	$(BUILD)/tests/test_sensors
	$(BUILD)/tests/test_heater
	$(BUILD)/tests/test_input
	$(BUILD)/tests/test_menu
	$(BUILD)/tests/test_protocol
	$(BUILD)/tests/test_settings
	$(BUILD)/tests/test_ui

preview: tools/screen_preview.c tools/host_display.h src/display.c src/font.c src/gfx.c src/fmt.c src/ui.c src/menu.c src/heater.c src/sensors.c src/settings.c src/board.c
	@mkdir -p $(BUILD)/preview
	$(CLANG) -std=c11 -O1 -Wall -Wextra -Werror -DOPENDWS_HOST_TEST=1 -Isrc -DSCREEN_PREVIEW_DIR='"$(BUILD)/preview"' $< src/display.c src/font.c src/gfx.c src/fmt.c src/ui.c src/menu.c src/heater.c src/sensors.c src/settings.c src/board.c -o $(BUILD)/preview/run
	$(BUILD)/preview/run

screenshots: preview tools/ppm_to_png.py
	$(PYTHON) tools/ppm_to_png.py $(BUILD)/preview docs/screenshots

clean:
	rm -rf $(BUILD) $(DIST)
