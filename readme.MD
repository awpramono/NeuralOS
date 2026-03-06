# 🧠 NeuralOS v3.6.5 — Agentic AI Operating System

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
│                                                                                  │
│ NeuralOS v3.6.2 - Agentic AI Operating System                                   │
│                                                                                  │
│ USER > berapa memory yang tersedia?                                              │
│ [Agent] Intent: SYSTEM_MEM (90%)                                                │
│ AI > [System Analysis] Memory Report:                                           │
│       Total RAM    : 512 MB                                                      │
│       Heap used    : 1693 KB                                                     │
│       AI Model RAM : ~1593 KB                                                    │
│                                                                                  │
│ USER > run 10 5 * 20 2 / - print                                                 │
│ [Sandbox] Assembling & Executing Ephemeral Core...                               │
│   --> [Out] 40                                                                   │
│ [Sandbox] Execution terminated. Operations: 8                                    │
│                                                                                  │
│ AI > [Proactive] Sejauh ini kita sudah menyelesaikan 3 task dengan sukses.       │
│ Ada yang bisa dibantu lagi?                                                      │
└──────────────────────────────────────────────────────────────────────────────────┘
```

## 🚀 Features

### 🧠 AI Stack (Kernel-Level)
| Feature | Description |
|---------|-------------|
| **Llama2 Transformer** | Full port of Karpathy's llama2.c — RMSNorm, RoPE, GQA, SwiGLU FFN, KV Cache |
| **Model Support** | stories260K (1MB, fast) and **stories15M (60MB, highly coherent)** |
| **AI Agent** | Intent classifier with 50+ keyword rules, goal-oriented success tracking |
| **Proactive Interaction** | Agent automatically suggests actions via idle timers, no typing needed |
| **AI Firewall** | Gatekeeping layer blocks destructive prompts (e.g., simulated rm -rf) |
| **Q8 Quantization** | 3.5x compression, dequantize-on-the-fly matmul |
| **x87 FPU Math** | Hardware FSIN/FCOS/F2XM1/FSQRT — 80-bit precision |

### 💻 Operating System
| Feature | Description |
|---------|-------------|
| **4-Core SMP + KVM** | APIC IPI wake, per-core stacks, parallel matmul + hardware acceleration |
| **VGA Scrolling** | Line-by-line scroll, hardware cursor, persistent status bar |
| **Non-blocking PS/2** | Async keyboard polling driver enables background OS/AI tasks |
| **NeuralFS** | Native bare-metal flat file system with superblocks directly on raw AT PIO disk |
| **Tiny C Compiler** | Fully-integrated Embedded C Interpreter (`neuralc`) for self-compilation of C code |
| **Ephemeral Sandbox** | In-kernel bytecode VM executes RPN logic (Stack Machine) on the fly |
| **COM1 Telemetry** | Background logging of system state and agent behavior to isolated serial port |
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

# With stories260K Llama2 (1MB, fast)
make run-llama
make run-q8          # Q8 quantized (3.5x smaller)

# With stories15M Llama2 (60MB, highly coherent!)
make run-15m
make run-15m-q8      # Q8 quantized (~17MB)
make run-15m-q8-kvm  # Hardware acceleration (Insane speed!)

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
| `ls` | SYSTEM_FS | NeuralFS renders directory listing |
| `tulis` | AI_STORY | Llama2 generates a story |
| `tcc void main(){print_string("Hi");}` | AI_CODE | NeuralC compiles structure inline |
| `run 10 5 * print` | AI_CODE | Ephemeral VM executes RPN Math (Output: 50) |

## 🛠️ Module Overview

| File | Description |
|------|-------------|
| `src/boot.s` | Multiboot entry, stack setup, jump to C kernel |
| `src/trampoline.s` | AP startup: per-core stack, FPU, APIC ID |
| `src/kernel.c` | Main kernel: boot sequence, idle timers, loop |
| **`src/agent.c`** | **AI Agent: intent classifier, firewall, goal-oriented dispatcher** |
| `src/llama2.c` | Llama2 transformer engine (full forward pass) |
| **`src/neuralc.c`** | **Embedded Tiny C Compiler / C4 interpreter** |
| **`src/fs.c`** | **NeuralFS: Superblocks and raw disk file storage** |
| **`src/vm.c`** | **Ephemeral Sandbox: Stack machine RPN interpreter** |
| `src/ai.c` | 32-token fast AI engine (GGUF) |
| `src/math.c` | x87 FPU math + Q8 matmul |
| `src/smp.c` | 4-core SMP initialization via APIC IPI |
| `src/memory.c` | Heap allocator (bump + calloc + memcpy) |
| `src/vga.c` | VGA driver with scrolling + status bar |
| `src/keyboard.c` | PS/2 keyboard non-blocking polling driver |
| `src/ide.c` | ATA PIO disk driver (multi-sector) |
| `src/serial.c` | COM1 Serial Port driver |
| `include/system.h` | All type defs and function prototypes |

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
| **Llama2 Models** | **stories260K (264K params) and stories15M (15.2M params)** |
| **Quantization** | **Q8_0: 36 bytes per 32 weights (3.5x compression)** |
| **Math Backend** | **x87 FPU: FSIN, FCOS, F2XM1, FSQRT, FYL2X / KVM** |
| **AI Agent** | **Goal-oriented tracker, AI Firewall, Proactive idle-timer** |
| 32-Token AI | 2,048 float params (8KB GGUF) |
| SMP Protocol | Local APIC, INIT+SIPI IPI |

## 🔮 Evolution Roadmap

### Phase 1: AI Agent Kernel ✅ Complete (100%)
- [x] Intent Classifier (50+ rules, bilingual)
- [x] AI Command Dispatch
- [x] Goal-Oriented System (Task success tracking)
- [x] Context-Aware System Reports
- [x] VGA Scrolling & Status Bar

### Phase 2: Smart Model Orchestration ⚙️ ✅ Complete (100%)
- [x] Larger model (stories15M — 15M params)
- [x] Multi-Model Support (stories260K vs 15M)
- [x] KVM Hardware Acceleration in QEMU
- [x] SmolLM-135M with Q8 quantization
- [x] Task-specific model routing

### Phase 3: Connected Edition 🌱 Early Dev (30%)
- [x] AI Firewall (Basic Gatekeeping / Destructive block)
- [x] E1000 network driver for QEMU
- [ ] Minimal TCP/IP stack
- [ ] HTTP client for cloud AI APIs

### Phase 4: Cognitive OS 🧠 In Progress (35%)
- [x] Proactive Interaction (Non-blocking keyboard, Idle Timer Agent)
- [x] Ephemeral Sandbox (Bare-metal Bytecode VM Interpreter)
- [ ] Simple filesystem (FAT12/16)
- [ ] Vector-based file search

## 📈 Version History

| Version | Milestone |
|---------|-----------|
| v1.0 | Single-core kernel, VGA, keyboard |
| v1.7 | Dual-core SMP |
| v3.0 | 4-core SMP, 32-token AI, interactive shell |
| v3.1 | Llama2 transformer engine ported |
| v3.2 | x87 FPU hardware math → coherent English |
| v3.3 | Prompt support, temperature sampling |
| v3.4 | Q8_0 quantization (3.5x compression) |
| v3.5 | AI Agent Kernel, VGA scrolling, status bar |
| **v3.6** | **Stories15M model (15.2M params), KVM Acceleration** |
| **v3.6.1**| **Proactive Agent (Idle Timer), Goal-Oriented tracking, AI Firewall** |
| **v3.6.2**| **Ephemeral Sandbox VM (RPN bytecodes)** |

## 📜 License

MIT License — See [LICENSE](LICENSE) file.

---

**Built from scratch for educational purposes in low-level systems engineering and AI inference.**
*No Linux. No libc. No dependencies. Just raw x86 and C.*
