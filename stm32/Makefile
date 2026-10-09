PREFIX ?= arm-none-eabi-
VERSION ?= 1
CC := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
SIZE := $(PREFIX)size
CFLAGS = -mcpu=cortex-m3 -mthumb -Os -std=gnu11 -Wall -Wextra -Wno-misleading-indentation -ffunction-sections -fdata-sections -fno-common -DFW_VERSION=$(VERSION) -Iinclude -I../common
SRC = src/startup.c src/main.c src/board.c src/config.c src/stage.c src/node.c ../common/protocol.c ../common/sha256.c
DEPS = $(wildcard include/*.h ../common/*.h ld/*.ld)
LDFLAGS = -nostartfiles --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections
all: build/boot.bin build/app.bin
build:
	mkdir -p build
build/boot.elf: $(SRC) $(DEPS) ../common/ota_key.h | build
	$(CC) $(CFLAGS) -DBOOTLOADER $(SRC) $(LDFLAGS) -Tld/boot.ld -Wl,-Map=build/boot.map -o $@
	$(SIZE) $@
build/app.elf: $(SRC) $(DEPS) ../common/ota_key.h | build
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -Tld/app.ld -Wl,-Map=build/app.map -o $@
	$(SIZE) $@
build/%.bin: build/%.elf
	$(OBJCOPY) -O binary $< $@
flash: all
	openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program build/boot.bin 0x08000000 verify" -c "program build/app.bin 0x08004000 verify reset exit"
clean:
	rm -rf build
.PHONY: all flash clean
