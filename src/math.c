#include "system.h"

// Variabel sinkronisasi global (Spinlock)
volatile int lock = 0;

// ============================================================================
// x87 FPU HARDWARE MATH — Uses CPU's built-in floating point unit
// Much more precise than Taylor series, critical for transformer inference
// ============================================================================

// exp(x) via x87 FPU: e^x = 2^(x * log2(e))
// Uses F2XM1 (computes 2^x - 1 for -1 <= x <= 1) + FSCALE
float exp_float(float x) {
    float result;
    __asm__ volatile (
        "flds    %1          \n\t"  // ST(0) = x
        "fldl2e              \n\t"  // ST(0) = log2(e), ST(1) = x
        "fmulp               \n\t"  // ST(0) = x * log2(e)
        "fld     %%st(0)     \n\t"  // duplicate: ST(0)=ST(1) = x*log2(e)
        "frndint             \n\t"  // ST(0) = round(x*log2(e)) = integer part
        "fsub    %%st(0), %%st(1) \n\t"  // ST(1) = fractional part
        "fxch    %%st(1)     \n\t"  // ST(0) = frac, ST(1) = int
        "f2xm1               \n\t"  // ST(0) = 2^frac - 1
        "fld1                \n\t"  // ST(0) = 1
        "faddp               \n\t"  // ST(0) = 2^frac
        "fscale              \n\t"  // ST(0) = 2^frac * 2^int = 2^(x*log2e) = e^x
        "fstp    %%st(1)     \n\t"  // clean up ST(1)
        "fstps   %0          \n\t"  // store result
        : "=m" (result)
        : "m" (x)
    );
    return result;
}

// sqrt(x) via x87 FSQRT
float sqrt_float(float x) {
    if (x <= 0.0f) return 0.0f;
    float result;
    __asm__ volatile (
        "flds   %1   \n\t"
        "fsqrt       \n\t"
        "fstps  %0   \n\t"
        : "=m" (result)
        : "m" (x)
    );
    return result;
}

// log(x) via x87 FYL2X: computes y * log2(x), we set y = log(2) = ln(2)
float log_float(float x) {
    if (x <= 0.0f) return -100.0f;
    float result;
    __asm__ volatile (
        "fldln2          \n\t"  // ST(0) = ln(2)
        "flds    %1      \n\t"  // ST(0) = x, ST(1) = ln(2)
        "fyl2x           \n\t"  // ST(0) = ln(2) * log2(x) = ln(x)
        "fstps   %0      \n\t"
        : "=m" (result)
        : "m" (x)
    );
    return result;
}

// pow(base, exp) = exp(exp * ln(base))
float powf_float(float base, float exponent) {
    if (base <= 0.0f) return 0.0f;
    return exp_float(exponent * log_float(base));
}

// sin(x) via x87 FSIN
float sin_float(float x) {
    float result;
    __asm__ volatile (
        "flds   %1   \n\t"
        "fsin        \n\t"
        "fstps  %0   \n\t"
        : "=m" (result)
        : "m" (x)
    );
    return result;
}

// cos(x) via x87 FCOS
float cos_float(float x) {
    float result;
    __asm__ volatile (
        "flds   %1   \n\t"
        "fcos        \n\t"
        "fstps  %0   \n\t"
        : "=m" (result)
        : "m" (x)
    );
    return result;
}

// sigmoid and SiLU
float sigmoid(float x) { 
    return 1.0f / (1.0f + exp_float(-x)); 
}
float silu(float x) { return x * sigmoid(x); }

// Softmax
void softmax(float *input, int size) { 
    float max_val = input[0]; 
    for(int i = 1; i < size; i++) if(input[i] > max_val) max_val = input[i]; 
    float sum = 0.0f; 
    for(int i = 0; i < size; i++) { input[i] = exp_float(input[i] - max_val); sum += input[i]; } 
    for(int i = 0; i < size; i++) input[i] /= sum; 
}

// RMSNorm
void rmsnorm(float* o, float* x, float* weight, int size) { 
    float sum_sq = 0.0f; 
    for(int j = 0; j < size; j++) sum_sq += x[j] * x[j]; 
    sum_sq /= size; sum_sq += 1e-5f; 
    float inv_sqrt = 1.0f / sqrt_float(sum_sq); 
    for(int j = 0; j < size; j++) o[j] = weight[j] * (inv_sqrt * x[j]); 
}

// Standard matmul (single-core fallback)
void matmul(float* xout, float* x, float* w, int n, int d) {
    for (int i = 0; i < d; i++) {
        float val = 0.0f;
        for (int j = 0; j < n; j++) val += w[i * n + j] * x[j];
        xout[i] = val;
    }
}

// N-way parallel matmul
void matmul_parallel(float* xout, float* x, float* w, int n, int d, int core_id, int num_cores) {
    for (int i = core_id; i < d; i += num_cores) {
        float val = 0.0f;
        for (int j = 0; j < n; j++) {
            val += w[i * n + j] * x[j];
        }
        xout[i] = val;
    }
}
