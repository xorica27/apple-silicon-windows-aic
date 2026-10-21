CROSS ?= aarch64-elf-
CC := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy

CFLAGS := -ffreestanding -fno-stack-protector -fno-pic -mgeneral-regs-only \
          -O2 -Wall -Wextra
LDFLAGS := -nostdlib -nostartfiles -T linker.ld -Wl,--build-id=none \
           -Wl,-Map,payload.map

all: payload.elf

payload.elf: start.S aic_payload.c linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ start.S aic_payload.c

payload.bin: payload.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f payload.elf payload.bin payload.map

.PHONY: all clean
