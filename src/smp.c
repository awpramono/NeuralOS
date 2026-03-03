#include "system.h"

extern uint8_t trampoline_start;
extern uint8_t trampoline_end;

// ============================================================================
// SMP MAILBOX - Shared memory for inter-core communication
//
// 0x9000  signal        (0=idle, 1=compute)
// 0x9004  xout          pointer to output vector
// 0x9008  x             pointer to input vector
// 0x900C  w             pointer to weight matrix
// 0x9010  n             input dimension
// 0x9014  d             output dimension
// 0x9018  num_cores     total number of active cores
// 0x901C  cores_booted  atomic counter (set by trampoline)
// 0x9020  workers_done  atomic counter (set by workers after matmul)
// 0x9100  worker_addr   ap_worker function pointer
// ============================================================================

#define SMP_SIGNAL       ((volatile uint32_t*)0x9000)
#define SMP_NUM_CORES    ((volatile uint32_t*)0x9018)
#define SMP_CORES_BOOTED ((volatile uint32_t*)0x901C)
#define SMP_WORKERS_DONE ((volatile uint32_t*)0x9020)
#define SMP_WORKER_PTR   ((volatile uint32_t*)0x9100)

// Read this core's APIC ID
static inline uint32_t get_apic_id() {
    return *(volatile uint32_t*)0xFEE00020 >> 24;
}

// ============================================================================
// AP WORKER - Runs on each worker core (Core 1, 2, 3)
// ============================================================================
void ap_worker() {
    uint32_t my_id = get_apic_id();
    
    while (1) {
        // Phase 1: Wait for work signal
        while (*SMP_SIGNAL != 0x1);
        
        // Read shared parameters and execute matmul for this core's rows
        uint32_t nc = *SMP_NUM_CORES;
        matmul_parallel(
            *(float* volatile*)0x9004,    // xout
            *(float* volatile*)0x9008,    // x
            *(float* volatile*)0x900C,    // w
            *(volatile int*)0x9010,       // n
            *(volatile int*)0x9014,       // d
            (int)my_id,                   // core_id (determines which rows)
            (int)nc                       // num_cores (stride)
        );
        
        // Signal this worker is done
        __sync_fetch_and_add(SMP_WORKERS_DONE, 1);
        
        // Phase 2: Wait for Core 0 to reset signal (prevents re-entry)
        while (*SMP_SIGNAL != 0x0);
    }
}

// ============================================================================
// INIT SMP - Wake up all available cores
// ============================================================================
void init_smp() {
    print_string("\n[SMP] Initializing multi-core system...\n", 0x0B);
    
    // 1. Initialize mailbox
    *SMP_SIGNAL = 0;
    *SMP_CORES_BOOTED = 0;
    *SMP_WORKERS_DONE = 0;
    *SMP_NUM_CORES = 1;  // Start with just Core 0
    
    // 2. Store worker function address for trampoline
    *SMP_WORKER_PTR = (uint32_t)&ap_worker;
    
    // 3. Copy trampoline to 0x8000
    uint8_t *dest = (uint8_t *)0x8000;
    uint8_t *src = &trampoline_start;
    uint32_t size = &trampoline_end - &trampoline_start;
    for(uint32_t i = 0; i < size; i++) dest[i] = src[i];
    print_string("[SMP] Trampoline installed at 0x8000.\n", 0x0A);
    
    // 4. Send INIT IPI to ALL other cores (broadcast)
    volatile uint32_t *lapic = (uint32_t *)0xFEE00000;
    lapic[0x300/4] = 0x000C4500;  // INIT IPI, All Excluding Self
    
    // Wait ~10ms
    for(volatile int i = 0; i < 1000000; i++);
    
    // 5. Send STARTUP IPI to ALL other cores (vector 0x08 = 0x8000)
    lapic[0x300/4] = 0x000C4608;  // SIPI, All Excluding Self, vector 0x08
    
    // Wait a bit, then send SIPI again (spec recommends twice)
    for(volatile int i = 0; i < 500000; i++);
    lapic[0x300/4] = 0x000C4608;
    
    print_string("[SMP] SIPI broadcast sent.\n", 0x0D);
    
    // 6. Wait for worker cores to boot (timeout after ~2 seconds)
    print_string("[SMP] Waiting for cores...", 0x0F);
    
    uint32_t last_count = 0;
    int stable_ticks = 0;
    for(int timeout = 0; timeout < 5000000; timeout++) {
        uint32_t current = *SMP_CORES_BOOTED;
        if (current != last_count) {
            last_count = current;
            stable_ticks = 0;
        } else {
            stable_ticks++;
        }
        // If count hasn't changed for a while, all cores have booted
        if (stable_ticks > 500000 && last_count > 0) break;
    }
    
    uint32_t num_workers = *SMP_CORES_BOOTED;
    uint32_t total_cores = num_workers + 1;  // +1 for Core 0
    *SMP_NUM_CORES = total_cores;
    
    if (num_workers > 0) {
        print_string(" [OK]\n", 0x0A);
        print_string("[SMP] Cores online: ", 0x0B);
        print_number(total_cores, 0x0E);
        print_string(" (1 main + ", 0x0B);
        print_number(num_workers, 0x0E);
        print_string(" workers)\n", 0x0B);
        print_string("[SMP] Parallel matmul: ", 0x0A);
        print_number(total_cores, 0x0E);
        print_string("-way split across all cores\n", 0x0A);
    } else {
        print_string(" [SINGLE]\n", 0x0C);
        print_string("[SMP] No worker cores detected. Single-core mode.\n", 0x0C);
    }
}
