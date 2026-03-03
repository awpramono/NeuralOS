CC = gcc
AS = as
LD = ld
CFLAGS = -m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Iinclude
LDFLAGS = -m elf_i386 -T linker.ld

OBJS = src/boot.o src/trampoline.o src/vga.o src/memory.o src/ide.o src/math.o src/smp.o src/ai.o src/kernel.o

all: myos.bin

myos.bin: $(OBJS)
	@$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@echo "[SUKSES] NeuralOS v1.7 Berhasil Dirakit!"

src/%.o: src/%.c
	@$(CC) $(CFLAGS) -c $< -o $@

src/boot.o: src/boot.s
	@$(AS) --32 $< -o $@

src/trampoline.o: src/trampoline.s
	@$(AS) --32 $< -o $@

run: myos.bin
	@dd if=/dev/zero of=disk.img bs=1M count=64 2>/dev/null
	@python3 scripts/inject_brain.py
	@qemu-system-i386 -kernel myos.bin -drive file=disk.img,format=raw,index=0,media=disk -smp 2 -m 512M

clean:
	@rm -f src/*.o myos.bin disk.img
