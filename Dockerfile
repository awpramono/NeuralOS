FROM ubuntu:22.04

# Matasi interaksi saat build container
ENV DEBIAN_FRONTEND=noninteractive

# Install semua kebutuhan kompilator (C89, QEMU, Python, dsb)
RUN apt-get update && apt-get install -y \
    gcc \
    gcc-multilib \
    make \
    binutils \
    qemu-system-x86 \
    python3 \
    && rm -rf /var/lib/apt/lists/*

# Buat direktori kerja OS
WORKDIR /neuralos

# Salin semua source code rakitan kita ke dalam kontainer
COPY . /neuralos/

# Rakit ulang Kernel ke versi terbaru di dalam Container
RUN make clean && make all

# Buat skrip penyalaan otonom (Jalankan AI Bridge + QEMU VNC Mode)
# QEMU akan berjalan "tanpa layar", tapi mengirimkan videonya via VNC port 5900
RUN echo '#!/bin/bash\n\
echo "[Docker] Menyalakan DeepSeek AI Bridge..."\n\
python3 scripts/deepseek_bridge.py &\n\
echo "[Docker] Menyiapkan Disk Storage Image..."\n\
dd if=/dev/zero of=disk.img bs=1M count=128 2>/dev/null >/dev/null\n\
echo "[Docker] Mem-Booting NeuralOS! Silakan sambungkan VNC ke Port 5900 (misal: localhost:5900)"\n\
qemu-system-i386 -kernel myos.bin -drive file=disk.img,format=raw,index=0,media=disk -smp 4 -m 512M -device e1000,netdev=n1 -netdev user,id=n1 -display vnc=0.0.0.0:0\n\
' > /neuralos/entrypoint.sh && chmod +x /neuralos/entrypoint.sh

# Buka gerbang layar VNC dari dalam Docker ke dunia luar (Host)
EXPOSE 5900

# Perintah pengeksekusi saat kontainer Docker berjalan
CMD ["/neuralos/entrypoint.sh"]
