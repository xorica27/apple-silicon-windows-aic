CROSS ?= aarch64-elf-
CC := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy

CFLAGS := -ffreestanding -fno-stack-protector -fno-pic -mgeneral-regs-only \
          -O2 -Wall -Wextra
LDFLAGS := -nostdlib -nostartfiles -T linker.ld -Wl,--build-id=none
LDFLAGS_HW := -nostdlib -nostartfiles -T linker-hw.ld -Wl,--build-id=none

all: payload-v1.elf payload-v2.elf

payload-v1.elf: start.S aic_payload.c board.h linker.ld
	$(CC) $(CFLAGS) -DBOARD=BOARD_QEMU_V1 $(LDFLAGS) \
		-Wl,-Map,$(@:.elf=.map) -o $@ start.S aic_payload.c

payload-v2.elf: start.S aic_payload.c board.h linker.ld
	$(CC) $(CFLAGS) -DBOARD=BOARD_QEMU_V2 $(LDFLAGS) \
		-Wl,-Map,$(@:.elf=.map) -o $@ start.S aic_payload.c

payload-m1pro.elf: start.S aic_payload.c board.h linker-hw.ld
	$(CC) $(CFLAGS) -DBOARD=BOARD_M1PRO $(LDFLAGS_HW) \
		-Wl,-Map,$(@:.elf=.map) -o $@ start.S aic_payload.c

payload-m1pro.bin: payload-m1pro.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f payload-v1.elf payload-v2.elf payload-m1pro.elf payload-m1pro.bin \
	      payload-v1.map payload-v2.map payload-m1pro.map

.PHONY: all clean
