# 🧠 NeuralOS v3.0 — Bare-Metal Tiny LLM Operating System

NeuralOS is a micro operating system built from scratch in C and x86 Assembly that runs a **Tiny Language Model (LLM) directly on bare metal** — no Linux kernel, no standard C library, no modern software stack. The neural network inference engine runs natively using custom math functions and true multi-core parallel computation.

## 🚀 Key Features

### Operating System
- **Custom 32-bit Kernel** — Bootable via Multiboot/QEMU, written in C + x86 Assembly
- **4-Core SMP** — Wakes up all CPU cores via APIC IPI, each with its own stack and FPU
- **Dynamic Memory Manager** — Bump allocator for heap management at 16MB+
- **VGA Text Driver** — Direct writes to physical address `0xB8000`
- **PS/2 Keyboard Driver** — Polling-based keyboard input with backspace support
- **ATA PIO Disk Driver** — Read sectors from virtual hard drive
- **Interactive Shell** — Built-in commands: `HELP`, `INFO`, `MEM`, `VER`, `CLEAR`

### AI / Neural Network
- **32-Token Vocabulary** — Covers system terms, greetings, and conversational tokens
- **2-Layer Neural Network** — Embedding matrix (32×32) + Projection matrix (32×32)
- **Full Inference Pipeline** — `Embedding → RMSNorm → MatMul → SiLU → Softmax → Argmax`
- **Parallel MatMul** — Matrix multiplication split across all available CPU cores
- **GGUF Format Support** — Parses GGUF model headers from disk
- **Bare-Metal Math** — Taylor series implementations of `exp`, `sqrt`, `sin`, `cos`, `sigmoid`, `silu`

### Multi-Core Architecture
```
┌──────────────────────────────────────────────────────────────┐
│               NeuralOS v3.0 — 4-Core AI Inference            │
├──────────────────────────────────────────────────────────────┤
│  Core 0 (BSP)  │  Kernel, Shell, AI Coordinator              │
│  Core 1 (AP)   │  Worker: matmul rows 1, 5, 9, 13, ...      │
│  Core 2 (AP)   │  Worker: matmul rows 2, 6, 10, 14, ...     │
│  Core 3 (AP)   │  Worker: matmul rows 3, 7, 11, 15, ...     │
├──────────────────────────────────────────────────────────────┤
│  Sync: SMP Mailbox @ 0x9000 | Atomic counters | 2-phase     │
└──────────────────────────────────────────────────────────────┘
```

## 📟 Quick Start

### Prerequisites
- Linux or WSL (Windows Subsystem for Linux)
- `gcc`, `as`, `ld` (GNU toolchain, 32-bit support)
- `qemu-system-i386`
- `python3`

### Build & Run
```bash
# Compile and launch in QEMU (4 cores, 512MB RAM)
make run

# Or build only
make

# Clean build artifacts
make clean
```

### Try These Commands
| Input | Response |
|-------|----------|
| `HELP` | Shows all OS commands |
| `INFO` | Displays system info (cores, RAM, etc.) |
| `MEM` | Shows heap memory status |
| `VER` | ASCII art version banner |
| `CLEAR` | Clears the screen |
| `HELLO` | AI: *"Halo! Saya adalah NeuralOS, asisten AI Anda."* |
| `TELL ME ABOUT AI` | AI: *"Saya adalah NeuralOS, asisten AI Anda."* |
| `CPU` | AI: *"CPU: 4-core aktif."* |
| `How much RAM?` | AI: *"Sistem memori: 512MB RAM tersedia."* |
| `?` | AI: *"Perintah: HELP INFO MEM CLEAR AI."* |

## 🛠️ Architecture

### Module Overview
| File | Description |
|------|-------------|
| `src/boot.s` | Multiboot entry point, stack setup, jump to C kernel |
| `src/trampoline.s` | AP (Application Processor) startup: per-core stack, FPU, APIC ID detection |
| `src/kernel.c` | Main kernel: shell, built-in commands, boot sequence |
| `src/ai.c` | LLM inference engine: vocabulary, weight init, autoregressive generation |
| `src/math.c` | Bare-metal math: exp, sqrt, sin, cos, sigmoid, silu, softmax, rmsnorm, matmul |
| `src/smp.c` | SMP initialization: APIC IPI, worker function, mailbox protocol |
| `src/memory.c` | Heap allocator (bump allocator) |
| `src/vga.c` | VGA text mode driver |
| `src/keyboard.c` | PS/2 keyboard polling driver |
| `src/ide.c` | ATA PIO disk read driver |
| `include/system.h` | System-wide type definitions, struct declarations, function prototypes |
| `scripts/make_gguf.py` | Generates GGUF model file with verified transition weights |
| `linker.ld` | Linker script: kernel loaded at 1MB |

### Neural Network Inference Flow
```
User Input: "HELLO"
    │
    ▼
┌─────────────────┐
│ Input Classifier │ ── Keyword matching → Starting token (0 = "Halo!")
└────────┬────────┘
         │
    ┌────▼────┐
    │ Embed   │ ── state = E[token, :] (32-dim vector from 32×32 matrix)
    │ Lookup  │
    └────┬────┘
         │
    ┌────▼────┐
    │ RMSNorm │ ── Normalize state vector (custom sqrt via Newton's method)
    └────┬────┘
         │
    ┌────▼─────────┐
    │ Parallel     │ ── logits = P × state (split across 4 CPU cores)
    │ MatMul (SMP) │    Core 0: rows 0,4,8,...  Core 1: rows 1,5,9,...
    └────┬─────────┘    Core 2: rows 2,6,10,... Core 3: rows 3,7,11,...
         │
    ┌────▼────┐
    │  SiLU   │ ── x × σ(x), using Taylor series exp()
    └────┬────┘
         │
    ┌────▼────┐
    │ Softmax │ ── Probability distribution over 32 tokens
    └────┬────┘
         │
    ┌────▼────┐
    │ Argmax  │ ── Pick highest probability → next token
    └────┬────┘
         │
         ▼
    Print token, repeat until [END]
```

## 📊 Technical Specifications

| Spec | Value |
|------|-------|
| Architecture | x86 (i386), 32-bit Protected Mode |
| Boot Protocol | Multiboot 1 |
| RAM | 512MB (configurable) |
| CPU Cores | 4 (configurable via `-smp N`) |
| VGA | 80×25 text mode |
| Disk | ATA PIO, 64MB disk image |
| Heap Start | 0x1000000 (16MB) |
| AI Vocab Size | 32 tokens |
| AI Dimensions | 32×32 embed + 32×32 projection |
| AI Parameters | 2,048 floats (8KB) |
| Math Precision | 32-bit float (x87 FPU) |
| SMP Protocol | Local APIC, INIT+SIPI IPI |

## 🔮 Roadmap

- [ ] **Bigram Character Model** — Generate text character-by-character (128×128 ASCII model)
- [ ] **Mini Transformer** — Self-Attention, RoPE, KV Cache on bare metal
- [ ] **AI System Agent** — AI can execute OS commands based on natural language
- [ ] **Port llama2.c** — Run actual TinyStories model on bare metal
- [ ] **Interrupt-driven keyboard** — Replace polling with IRQ-based input
- [ ] **Virtual memory** — Paging and memory protection
- [ ] **Simple filesystem** — Read files from disk

## 📜 License

MIT License — See [LICENSE](LICENSE) file.

Built for educational purposes in low-level systems engineering and AI.
