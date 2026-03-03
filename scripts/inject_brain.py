import struct

# Kita akan menyuntikkan data ke Sektor 100
# 1 Sektor = 512 Byte. 
# Kita akan mengisi 4 angka float (16 byte) sebagai 'Intelligence Trigger'
sector_size = 512
offset = 100 * sector_size

# Data: Angka-angka ini akan dibaca oleh ai.c sebagai penggerak logit
# Contoh: [0.1, 0.9, 0.05, 0.0] -> Memberikan dorongan kuat pada token ID 1 ("MENYALA")
intelligence_data = [0.1, 0.5, 0.4, 0.0]
binary_data = struct.pack('ffff', *intelligence_data)

# Padding sisa sektor dengan nol agar pas 512 byte
padding = b'\x00' * (sector_size - len(binary_data))

with open('disk.img', 'r+b') as f:
    f.seek(offset)
    f.write(binary_data + padding)
    print(f"[*] Pengetahuan AI berhasil disuntikkan ke Sektor 100!")
