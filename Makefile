CC = gcc
AS = as
LD = ld
CFLAGS = -m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Iinclude
LDFLAGS = -m elf_i386 -T linker.ld

OBJS = src/boot.o src/trampoline.o src/vga.o src/memory.o src/ide.o src/math.o src/smp.o src/keyboard.o src/ai.o src/llama2.o src/kernel.o

all: myos.bin

myos.bin: $(OBJS)
	@$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@echo "[SUKSES] NeuralOS v3.0 Berhasil Dirakit!"

src/%.o: src/%.c
	@$(CC) $(CFLAGS) -c $< -o $@

src/boot.o: src/boot.s
	@$(AS) --32 $< -o $@

src/trampoline.o: src/trampoline.s
	@$(AS) --32 $< -o $@

run: myos.bin
	@dd if=/dev/zero of=disk.img bs=1M count=128 2>/dev/null
	@python3 scripts/make_gguf.py
	@qemu-system-i386 -kernel myos.bin -drive file=disk.img,format=raw,index=0,media=disk -smp 4 -m 512M

# Run with Llama2 model (downloads stories15M.bin on first run)
run-llama: myos.bin
	@python3 scripts/prepare_model.py
	@qemu-system-i386 -kernel myos.bin -drive file=disk.img,format=raw,index=0,media=disk -smp 4 -m 512M

clean:
	@rm -f src/*.o myos.bin disk.img

.PHONY: all run run-llama clean
