#include "system.h"

// Variabel sinkronisasi global (Spinlock)
volatile int lock = 0;

float exp_float(float x) { 
    // Clamp input to prevent Taylor series overflow
    if (x > 20.0f) return 485165195.0f;  // ~e^20
    if (x < -20.0f) return 0.0f;
    float sum = 1.0f, term = 1.0f; 
    for(int i = 1; i < 20; i++) { term = term * x / i; sum += term; } 
    return sum; 
}

float sqrt_float(float x) { 
    if(x <= 0.0f) return 0.0f; 
    float guess = x / 2.0f; 
    for(int i = 0; i < 10; i++) guess = 0.5f * (guess + x / guess); 
    return guess; 
}

float sin_float(float x) { 
    float sum = x, term = x; 
    for(int i = 1; i <= 7; i++) { term = -term * x * x / ((2 * i) * (2 * i + 1)); sum += term; } 
    return sum; 
}

float cos_float(float x) { 
    float sum = 1.0f, term = 1.0f; 
    for(int i = 1; i <= 7; i++) { term = -term * x * x / ((2 * i - 1) * (2 * i)); sum += term; } 
    return sum; 
}

float sigmoid(float x) { 
    // Short-circuit for large values to avoid exp overflow
    if (x > 15.0f) return 1.0f;
    if (x < -15.0f) return 0.0f;
    return 1.0f / (1.0f + exp_float(-x)); 
}
float silu(float x) { return x * sigmoid(x); }

void softmax(float *input, int size) { 
    float max_val = input[0]; 
    for(int i = 1; i < size; i++) if(input[i] > max_val) max_val = input[i]; 
    float sum = 0.0f; 
    for(int i = 0; i < size; i++) { input[i] = exp_float(input[i] - max_val); sum += input[i]; } 
    for(int i = 0; i < size; i++) input[i] /= sum; 
}

void rmsnorm(float* o, float* x, float* weight, int size) { 
    float sum_sq = 0.0f; 
    for(int j = 0; j < size; j++) sum_sq += x[j] * x[j]; 
    sum_sq /= size; sum_sq += 1e-5f; 
    float inv_sqrt = 1.0f / sqrt_float(sum_sq); 
    for(int j = 0; j < size; j++) o[j] = weight[j] * (inv_sqrt * x[j]); 
}

// MATMUL PARALEL: Membagi beban kerja berdasarkan ID Core
void matmul(float* xout, float* x, float* w, int n, int d) {
    // Implementasi standar (fallback jika SMP belum aktif)
    for (int i = 0; i < d; i++) {
        float val = 0.0f;
        for (int j = 0; j < n; j++) val += w[i * n + j] * x[j];
        xout[i] = val;
    }
}

// N-way parallel matmul: each core computes its assigned rows
// With 4 cores: Core 0 -> rows 0,4,8,...  Core 1 -> rows 1,5,9,...  etc.
void matmul_parallel(float* xout, float* x, float* w, int n, int d, int core_id, int num_cores) {
    for (int i = core_id; i < d; i += num_cores) {
        float val = 0.0f;
        for (int j = 0; j < n; j++) {
            val += w[i * n + j] * x[j];
        }
        // No lock needed: each core writes to unique indices
        xout[i] = val;
    }
}
