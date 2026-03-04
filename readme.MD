# 🧠 NeuralOS v3.5 — Agentic AI Operating System

**NeuralOS** is a micro operating system built from scratch that runs a **real Llama2 Transformer and AI Agent directly on bare metal** — no Linux, no libc, no modern software stack. It features intent-driven natural language processing, Q8 quantized inference, 4-core SMP parallel computation, and an AI Agent kernel that understands what you *mean*, not just what you type.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│ NeuralOS v3.5 Agentic | Cores:4 | Mem:1693KB | AI:Llama2           Agent ON     │
├──────────────────────────────────────────────────────────────────────────────────┤
│  _   _                      _  ___  ____                                        │
│ | \ | | ___ _   _ _ __ __ _| |/ _ \/ ___|                                       │
│ |  \| |/ _ \ | | | '__/ _` | | | | \___ \                                       │
│ | |\  |  __/ |_| | | | (_| | | |_| |___) |                                      │
│ |_| \_|\___|\__,_|_|  \__,_|_|\___/|____/                                       │
│                                                                                  │
│ NeuralOS v3.5 - Agentic AI Operating System                                     │
│                                                                                  │
│ USER > berapa memory yang tersedia?                                              │
│ [Agent] Intent: SYSTEM_MEM (90%)                                                │
│ AI > [System Analysis] Memory Report:                                           │
│       Total RAM    : 512 MB                                                      │
│       Heap used    : 1693 KB                                                     │
│       AI Model RAM : ~1593 KB                                                    │
│                                                                                  │
│ USER > tell me a story about a brave cat                                         │
│ [Agent → Llama2 Creative Engine]                                                │
│ AI > Once upon a time, there was a little cat named Lily. She loved...           │
└──────────────────────────────────────────────────────────────────────────────────┘
```

## 🚀 Features

### 🧠 AI Stack (Kernel-Level)
| Feature | Description |
|---------|-------------|
| **Llama2 Transformer** | Full port of Karpathy's llama2.c — RMSNorm, RoPE, GQA, SwiGLU FFN, KV Cache |
| **AI Agent** | Intent classifier with 50+ keyword rules, routes to correct engine |
| **Q8 Quantization** | 3.5x compression, dequantize-on-the-fly matmul |
| **Temperature Sampling** | XorShift32 RNG for text variety |
| **Prompt Support** | User types text → model continues with context |
| **x87 FPU Math** | Hardware FSIN/FCOS/F2XM1/FSQRT — 80-bit precision |
| **32-Token Fast Engine** | GGUF-based simple AI for system queries |

### 💻 Operating System
| Feature | Description |
|---------|-------------|
| **4-Core SMP** | APIC IPI wake, per-core stacks, parallel matmul |
| **VGA Scrolling** | Line-by-line scroll, hardware cursor, persistent status bar |
| **Status Bar** | Real-time display: cores, memory, AI model, agent status |
| **PS/2 Keyboard** | Polling driver with backspace support |
| **ATA PIO Disk** | Multi-sector reads, progress reporting for large files |
| **Bump Allocator** | Heap at 16MB, supports calloc/memcpy/memset |

### 🏗️ Architecture
```
┌──────────────────────────────────────────────────────────────┐
│                     USER INPUT                                │
│          "berapa memory?" / "tell me a story"                │
└───────────────────────┬──────────────────────────────────────┘
                        ▼
┌──────────────────────────────────────────────────────────────┐
│              AI AGENT (Intent Classifier)                     │
│  50+ keyword rules, multi-signal scoring, bilingual          │
│  → SYSTEM_MEM / SYSTEM_CPU / AI_STORY / AI_CHAT / ...       │
└──────────┬────────────┬──────────────┬───────────────────────┘
           ▼            ▼              ▼
    ┌──────────┐  ┌──────────┐  ┌──────────────┐
    │ Kernel   │  │ 32-Token │  │ Llama2       │
    │ Reports  │  │ Engine   │  │ Transformer  │
    │ (live)   │  │ (fast)   │  │ (creative)   │
    └──────────┘  └──────────┘  └──────────────┘
                                       │
                        ┌──────────────┤
                        ▼              ▼
                  ┌──────────┐  ┌──────────────┐
                  │ x87 FPU  │  │ 4-Core SMP   │
                  │ Hardware │  │ Parallel     │
                  │ Math     │  │ MatMul       │
                  └──────────┘  └──────────────┘
```

## 📟 Quick Start

### Prerequisites
```bash
# Ubuntu / WSL
sudo apt install gcc binutils qemu-system-x86 python3 nasm
sudo apt install gcc-multilib   # for 32-bit support
```

### Build & Run

```bash
# Simple build + run (32-token AI only)
make run

# With Llama2 Transformer (downloads stories260K on first run)
make run-llama

# With Q8 quantized model (3.5x compression)
make run-q8

# Build only
make

# Clean
make clean
```

### Try These Inputs

**Natural Language (AI Agent understands intent):**
| Input | Intent | Response |
|-------|--------|----------|
| `berapa memory yang tersedia?` | SYSTEM_MEM | Live memory analysis report |
| `ada berapa core processor?` | SYSTEM_CPU | CPU info with SMP details |
| `bersihkan layar` | SYSTEM_CLEAR | Clears screen |
| `apa saja yang bisa dilakukan?` | SYSTEM_HELP | Feature list |
| `version` | SYSTEM_VER | Version info |
| `tell me a story about a dog` | AI_STORY | Llama2 generates a story |
| `ceritakan dongeng` | AI_STORY | Llama2 creative engine |
| `LLAMA` | Direct | Raw Llama2 random generation |

## 🛠️ Module Overview

| File | Description |
|------|-------------|
| `src/boot.s` | Multiboot entry, stack setup, jump to C kernel |
| `src/trampoline.s` | AP startup: per-core stack, FPU, APIC ID |
| `src/kernel.c` | Main kernel: boot sequence, LLAMA direct command |
| **`src/agent.c`** | **AI Agent: intent classifier, command dispatcher** |
| **`src/llama2.c`** | **Llama2 transformer engine (full forward pass)** |
| `src/ai.c` | 32-token fast AI engine (GGUF) |
| `src/math.c` | x87 FPU math + Q8 matmul |
| `src/smp.c` | 4-core SMP initialization via APIC IPI |
| `src/memory.c` | Heap allocator (bump + calloc + memcpy) |
| `src/vga.c` | VGA driver with scrolling + status bar |
| `src/keyboard.c` | PS/2 keyboard polling driver |
| `src/ide.c` | ATA PIO disk driver (multi-sector) |
| `include/system.h` | All type defs and function prototypes |
| `scripts/prepare_model.py` | Download + inject model into disk.img |
| `scripts/quantize.py` | Float32 → Q8 quantizer (pure Python) |
| `scripts/make_gguf.py` | Generate 32-token GGUF brain |

## 📊 Technical Specifications

| Spec | Value |
|------|-------|
| Architecture | x86 (i386), 32-bit Protected Mode |
| Boot Protocol | Multiboot 1 |
| RAM | 512MB (configurable) |
| CPU Cores | 4 (configurable via `-smp N`) |
| VGA | 80×25 text mode with scrolling |
| Disk | ATA PIO, 128MB disk image |
| Heap Start | `0x1000000` (16MB) |
| **Llama2 Model** | **stories260K: 264K params, 5 layers, 8 heads** |
| **Quantization** | **Q8_0: 36 bytes per 32 weights (3.5x compression)** |
| **Math Backend** | **x87 FPU: FSIN, FCOS, F2XM1, FSQRT, FYL2X** |
| **AI Agent** | **50+ keyword rules, bilingual (EN/ID)** |
| 32-Token AI | 2,048 float params (8KB GGUF) |
| SMP Protocol | Local APIC, INIT+SIPI IPI |

## 🔮 Evolution Roadmap

### Phase 1: AI Agent Kernel ✅ Complete
- [x] Intent Classifier (50+ rules, bilingual)
- [x] AI Command Dispatch
- [x] Multi-Engine Router (fast/transformer)
- [x] Context-Aware System Reports
- [x] VGA Scrolling & Status Bar

### Phase 2: Smart Model Orchestration 🔨 Next
- [ ] Larger model (stories15M — 15M params)
- [ ] SmolLM-135M with Q8 quantization
- [ ] Task-specific model routing
- [ ] Parallel inference across cores

### Phase 3: Connected Edition
- [ ] E1000 network driver for QEMU
- [ ] Minimal TCP/IP stack
- [ ] HTTP client for cloud AI APIs

### Phase 4: Cognitive OS
- [ ] Simple filesystem (FAT12/16)
- [ ] Ephemeral program compilation
- [ ] Vector-based file search
- [ ] Proactive AI assistant

## 📈 Version History

| Version | Milestone |
|---------|-----------|
| v1.0 | Single-core kernel, VGA, keyboard |
| v1.7 | Dual-core SMP |
| v3.0 | 4-core SMP, 32-token AI, interactive shell |
| v3.1 | Llama2 transformer engine ported |
| v3.2 | x87 FPU hardware math → coherent English |
| v3.3 | Prompt support, temperature sampling |
| **v3.4** | **Q8_0 quantization (3.5x compression)** |
| **v3.5** | **AI Agent Kernel, VGA scrolling, status bar** |

## 📜 License

MIT License — See [LICENSE](LICENSE) file.

---

**Built from scratch for educational purposes in low-level systems engineering and AI inference.**
*No Linux. No libc. No dependencies. Just raw x86 and C.*
