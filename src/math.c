#include "system.h"

float exp_float(float x) { float sum=1.0f,term=1.0f; for(int i=1;i<20;i++){term=term*x/i;sum+=term;} return sum; }
float sqrt_float(float x) { if(x<=0.0f)return 0.0f; float guess=x/2.0f; for(int i=0;i<10;i++)guess=0.5f*(guess+x/guess); return guess; }
float sin_float(float x) { float sum=x,term=x; for(int i=1;i<=7;i++){term=-term*x*x/((2*i)*(2*i+1));sum+=term;} return sum; }
float cos_float(float x) { float sum=1.0f,term=1.0f; for(int i=1;i<=7;i++){term=-term*x*x/((2*i-1)*(2*i));sum+=term;} return sum; }
void softmax(float *input, int size) { float max_val=input[0]; for(int i=1;i<size;i++)if(input[i]>max_val)max_val=input[i]; float sum=0.0f; for(int i=0;i<size;i++){input[i]=exp_float(input[i]-max_val);sum+=input[i];} for(int i=0;i<size;i++)input[i]/=sum; }
void rmsnorm(float* o, float* x, float* weight, int size) { float sum_sq=0.0f; for(int j=0;j<size;j++)sum_sq+=x[j]*x[j]; sum_sq/=size; sum_sq+=1e-5f; float inv_sqrt=1.0f/sqrt_float(sum_sq); for(int j=0;j<size;j++)o[j]=weight[j]*(inv_sqrt*x[j]); }
void matmul(float* xout, float* x, float* w, int n, int d) { for(int i=0;i<d;i++){float val=0.0f;for(int j=0;j<n;j++)val+=w[i*n+j]*x[j];xout[i]=val;} }
void attention(float* out, float* q, float* k, float* v, int seq_len, int d) { float scale=1.0f/sqrt_float((float)d); for(int i=0;i<seq_len;i++){ float scores[10]; for(int j=0;j<seq_len;j++){ float dot=0.0f; for(int x=0;x<d;x++)dot+=q[i*d+x]*k[j*d+x]; scores[j]=dot*scale; } softmax(scores,seq_len); for(int x=0;x<d;x++){ float val=0.0f; for(int j=0;j<seq_len;j++)val+=scores[j]*v[j*d+x]; out[i*d+x]=val; } } }

// GERBANG LOGIKA AKTIVASI (SiLU)
float sigmoid(float x) {
    return 1.0f / (1.0f + exp_float(-x));
}

float silu(float x) {
    return x * sigmoid(x);
}
