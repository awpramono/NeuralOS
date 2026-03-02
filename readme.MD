# 🧠 NeuralOS v1.3: Bare-Metal Transformer Engine

NeuralOS adalah sistem operasi mikro 32-bit yang dibangun dari nol untuk mendemonstrasikan eksekusi inferensi Kecerdasan Buatan (LLM) langsung di atas silikon (bare-metal), tanpa kernel Linux, tanpa library C standar, dan tanpa perantara software modern.



## 🚀 Pencapaian Teknis
- **Custom 32-bit Kernel:** Ditulis dalam C dan Assembly, bootable via GRUB/QEMU menggunakan protokol Multiboot.
- **Zero-Copy GGUF Parser:** Membedah file model AI langsung di RAM fisik untuk efisiensi latensi absolut.
- **Bare-Metal Math Cortex:** Implementasi mandiri Deret Taylor untuk fungsi `exp`, `sin`, `cos`, dan `sqrt`.
- **Transformer Blocks:** Mendukung RMSNorm, Rotary Positional Embedding (RoPE), Self-Attention, dan aktivasi SiLU.
- **Dynamic Memory Manager:** Implementasi *Bump Allocator* untuk manajemen KV Cache secara dinamis.
- **Autoregressive Inference:** Mampu menghasilkan teks secara mandiri melalui siklus inferensi terpadu.

## 🛠️ Arsitektur Modular
Proyek ini dipecah menjadi beberapa modul inti untuk skalabilitas industri:
- `vga.c`: Driver teks manual ke alamat fisik `0xB8000`.
- `memory.c`: Alokasi heap dinamis pada memori tinggi (16MB+).
- `math.c`: Mesin kalkulus desimal untuk operasi tensor.
- `ai.c`: Jantung inferensi (Attention & Inference Loop).

## 📟 Cara Menjalankan
Pastikan Anda memiliki `gcc`, `as`, `ld`, dan `qemu-system-i386` terpasang di sistem Linux/WSL Anda.

1. Kompilasi dan luncurkan:
   ```bash
   make run
2. Di dalam shell NeuralOS, ketik:
   ai
3. Saksikan silikon menghasilkan kalimat: "SILIKON MENYALA DUNIA."


MIT License - Dibuat untuk tujuan edukasi rekayasa sistem tingkat rendah.

---


