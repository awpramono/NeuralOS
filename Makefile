CC = gcc
AS = as
LD = ld
CFLAGS = -m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Iinclude
LDFLAGS = -m elf_i386 -T linker.ld

OBJS = src/boot.o src/trampoline.o src/vga.o src/memory.o src/ide.o src/math.o src/smp.o src/keyboard.o src/ai.o src/llama2.o src/agent.o src/vm.o src/serial.o src/kernel.o

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
	@qemu-system-i386 -kernel myos.bin -drive file=disk.img,format=raw,index=0,media=disk -smp 4 -m 512M -serial file:serial.log

# Run with Llama2 model (downloads stories260K on first run)
run-llama: myos.bin
	@python3 scripts/prepare_model.py
	@qemu-system-i386 -kernel myos.bin -drive file=disk.img,format=raw,index=0,media=disk -smp 4 -m 512M -serial file:serial.log

# Run with stories15M (60MB model, 32000 tokens — much better output!)
run-15m: myos.bin
	@python3 scripts/prepare_model.py stories15M
	@qemu-system-i386 -kernel myos.bin -drive file=disk.img,format=raw,index=0,media=disk -smp 4 -m 1G -serial file:serial.log

# Run with stories15M Q8 (~17MB, 3.5x compression)
run-15m-q8: myos.bin
	@python3 scripts/prepare_model.py stories15M --q8
	@qemu-system-i386 -kernel myos.bin -drive file=disk.img,format=raw,index=0,media=disk -smp 4 -m 1G -serial file:serial.log

# Run with KVM hardware acceleration (MUCH FASTER for 15M model)
run-15m-kvm: myos.bin
	@python3 scripts/prepare_model.py stories15M
	@qemu-system-i386 -kernel myos.bin -drive file=disk.img,format=raw,index=0,media=disk -smp 4 -m 1G -enable-kvm -cpu host -serial file:serial.log

run-15m-q8-kvm: myos.bin
	@python3 scripts/prepare_model.py stories15M --q8
	@qemu-system-i386 -kernel myos.bin -drive file=disk.img,format=raw,index=0,media=disk -smp 4 -m 1G -enable-kvm -cpu host -serial file:serial.log

# Run with Q8 quantized model (~3.5x compression)
run-q8: myos.bin
	@python3 scripts/prepare_model.py --q8
	@qemu-system-i386 -kernel myos.bin -drive file=disk.img,format=raw,index=0,media=disk -smp 4 -m 512M -serial file:serial.log

clean:
	@rm -f src/*.o myos.bin disk.img

.PHONY: all run run-llama run-15m run-15m-q8 run-q8 clean

