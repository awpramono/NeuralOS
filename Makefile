CC = gcc
AS = as
LD = ld
CFLAGS = -m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Iinclude
LDFLAGS = -m elf_i386 -T linker.ld

# Objek dengan jalur folder
OBJS = src/boot.o src/vga.o src/memory.o src/math.o src/ai.o src/kernel.o

all: myos.bin

myos.bin: $(OBJS)
@echo "[Linker] Menyusun silikon menjadi myos.bin..."
@$(LD) $(LDFLAGS) -o $@ $(OBJS)
@echo "[SUKSES] NeuralOS v1.3 Berhasil Dirakit!"

src/%.o: src/%.c
@echo "[GCC] Merakit $<..."
@$(CC) $(CFLAGS) -c $< -o $@

src/boot.o: src/boot.s
@echo "[AS] Merakit bootloader..."
@$(AS) --32 $< -o $@

run: myos.bin
@echo "[Python] Membuat Dummy GGUF Payload..."
@python3 -c "import struct; open('model.gguf', 'wb').write(struct.pack('<4sIQQfff', b'GGUF', 3, 1, 0, 1.5, 2.0, -0.5))"
@echo "[QEMU] Meluncurkan NeuralOS..."
@qemu-system-i386 -kernel myos.bin -initrd model.gguf -m 512M

clean:
@rm -f src/*.o myos.bin model.gguf
