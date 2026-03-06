#include "system.h"

// ============================================================================
// NeuralOS - Bare-Metal Llama2 Transformer Engine
// Ported from Andrej Karpathy's llama2.c
// No stdlib, no libc — pure bare-metal inference
// ============================================================================

// ----------------------------------------------------------------------------
// Transformer data structures

typedef struct {
  int dim;        // transformer dimension
  int hidden_dim; // FFN hidden dimension
  int n_layers;   // number of transformer layers
  int n_heads;    // number of query heads
  int n_kv_heads; // number of key/value heads
  int vocab_size; // vocabulary size
  int seq_len;    // max sequence length
} LlamaConfig;

typedef struct {
  float *token_embedding_table; // (vocab_size, dim)
  float *rms_att_weight;        // (layer, dim)
  float *rms_ffn_weight;        // (layer, dim)
  float *wq;                    // (layer, dim, n_heads * head_size)
  float *wk;                    // (layer, dim, n_kv_heads * head_size)
  float *wv;                    // (layer, dim, n_kv_heads * head_size)
  float *wo;                    // (layer, n_heads * head_size, dim)
  float *w1;                    // (layer, hidden_dim, dim)
  float *w2;                    // (layer, dim, hidden_dim)
  float *w3;                    // (layer, hidden_dim, dim)
  float *rms_final_weight;      // (dim,)
  float *wcls;                  // classifier weights
} LlamaWeights;

typedef struct {
  float *x;           // activation at current time stamp (dim,)
  float *xb;          // inside a residual branch (dim,)
  float *xb2;         // additional buffer (dim,)
  float *hb;          // FFN hidden buffer (hidden_dim,)
  float *hb2;         // FFN hidden buffer 2 (hidden_dim,)
  float *q;           // query (dim,)
  float *k;           // key (kv_dim,)
  float *v;           // value (kv_dim,)
  float *att;         // attention scores (n_heads, seq_len)
  float *logits;      // output logits (vocab_size,)
  float *key_cache;   // (layer, seq_len, kv_dim)
  float *value_cache; // (layer, seq_len, kv_dim)
} LlamaState;

// Global transformer state
static LlamaConfig g_config;
static LlamaWeights g_weights;
static LlamaState g_state;
static int g_model_loaded = 0;
static int g_quantized = 0; // 0=float32, 1=Q8

// Q8 weight pointers (used when g_quantized=1)
// These point to Q8Block data; the float* pointers in g_weights are reused as
// void* We cast them to uint8_t* during matmul_q8 calls

// Simple tokenizer: vocab table loaded from model
static char **g_vocab = 0;
static float *g_vocab_scores = 0;

// ----------------------------------------------------------------------------
// Map weight pointers into a contiguous data block

static void map_weights(LlamaWeights *w, LlamaConfig *p, float *ptr,
                        int shared_weights) {
  int head_size = p->dim / p->n_heads;
  int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
  int n_layers = p->n_layers;

  w->token_embedding_table = ptr;
  ptr += p->vocab_size * p->dim;
  w->rms_att_weight = ptr;
  ptr += n_layers * p->dim;
  w->wq = ptr;
  ptr += n_layers * p->dim * (p->n_heads * head_size);
  w->wk = ptr;
  ptr += n_layers * p->dim * kv_dim;
  w->wv = ptr;
  ptr += n_layers * p->dim * kv_dim;
  w->wo = ptr;
  ptr += n_layers * (p->n_heads * head_size) * p->dim;
  w->rms_ffn_weight = ptr;
  ptr += n_layers * p->dim;
  w->w1 = ptr;
  ptr += n_layers * p->dim * p->hidden_dim;
  w->w2 = ptr;
  ptr += n_layers * p->hidden_dim * p->dim;
  w->w3 = ptr;
  ptr += n_layers * p->dim * p->hidden_dim;
  w->rms_final_weight = ptr;
  ptr += p->dim;
  ptr += p->seq_len * head_size / 2; // skip freq_cis_real (legacy)
  ptr += p->seq_len * head_size / 2; // skip freq_cis_imag (legacy)
  w->wcls = shared_weights ? w->token_embedding_table : ptr;
}

// Q8 helper: number of Q8 bytes for a tensor of n_elements
static uint32_t q8_size(uint32_t n_elements) {
  return ((n_elements + 31) / 32) * 36; // 36 bytes per block of 32
}

// Map weight pointers for Q8 model
// Embedding table and RMS norm weights stay float32 (small, need precision)
// Big matmul weights (wq,wk,wv,wo,w1,w2,w3,wcls) are Q8
static void map_weights_q8(LlamaWeights *w, LlamaConfig *p, uint8_t *ptr,
                           int shared_weights) {
  int head_size = p->dim / p->n_heads;
  int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
  int n_layers = p->n_layers;

  // Token embedding: float32 (need precision for lookup)
  w->token_embedding_table = (float *)ptr;
  ptr += p->vocab_size * p->dim * sizeof(float);

  // RMS norm weights: float32 (small, 1D vectors)
  w->rms_att_weight = (float *)ptr;
  ptr += n_layers * p->dim * sizeof(float);

  // QKV weights: Q8 (big matrices)
  w->wq = (float *)ptr;
  ptr += n_layers * q8_size(p->dim * (p->n_heads * head_size));
  w->wk = (float *)ptr;
  ptr += n_layers * q8_size(p->dim * kv_dim);
  w->wv = (float *)ptr;
  ptr += n_layers * q8_size(p->dim * kv_dim);
  w->wo = (float *)ptr;
  ptr += n_layers * q8_size((p->n_heads * head_size) * p->dim);

  // FFN RMS norm: float32 (small)
  w->rms_ffn_weight = (float *)ptr;
  ptr += n_layers * p->dim * sizeof(float);

  // FFN weights: Q8 (big matrices)
  w->w1 = (float *)ptr;
  ptr += n_layers * q8_size(p->dim * p->hidden_dim);
  w->w2 = (float *)ptr;
  ptr += n_layers * q8_size(p->hidden_dim * p->dim);
  w->w3 = (float *)ptr;
  ptr += n_layers * q8_size(p->dim * p->hidden_dim);

  // Final RMS norm: float32
  w->rms_final_weight = (float *)ptr;
  ptr += p->dim * sizeof(float);

  // Classifier: Q8 or shared with embedding
  if (shared_weights) {
    w->wcls = w->token_embedding_table;
  } else {
    w->wcls = (float *)ptr;
    // ptr += q8_size(p->vocab_size * p->dim);  // not needed, last tensor
  }
}

// ----------------------------------------------------------------------------
// Allocate RunState buffers

static void alloc_state(LlamaState *s, LlamaConfig *p) {
  int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
  s->x = (float *)mem_calloc(p->dim, sizeof(float));
  s->xb = (float *)mem_calloc(p->dim, sizeof(float));
  s->xb2 = (float *)mem_calloc(p->dim, sizeof(float));
  s->hb = (float *)mem_calloc(p->hidden_dim, sizeof(float));
  s->hb2 = (float *)mem_calloc(p->hidden_dim, sizeof(float));
  s->q = (float *)mem_calloc(p->dim, sizeof(float));
  s->k = (float *)mem_calloc(kv_dim, sizeof(float));
  s->v = (float *)mem_calloc(kv_dim, sizeof(float));
  s->att = (float *)mem_calloc(p->n_heads * p->seq_len, sizeof(float));
  s->logits = (float *)mem_calloc(p->vocab_size, sizeof(float));
  s->key_cache =
      (float *)mem_calloc(p->n_layers * p->seq_len * kv_dim, sizeof(float));
  s->value_cache =
      (float *)mem_calloc(p->n_layers * p->seq_len * kv_dim, sizeof(float));
}

// ----------------------------------------------------------------------------
// Q8-aware matmul dispatch
// For float32: offset is element count. For Q8: offset is q8_size(elements)
static void do_matmul(float *out, float *in, float *w_ptr, int n, int d,
                      int layer, int tensor_elements) {
  if (g_quantized) {
    uint8_t *base = (uint8_t *)w_ptr;
    matmul_q8(out, in, base + (uint32_t)layer * q8_size(tensor_elements), n, d);
  } else {
    matmul(out, in, w_ptr + (uint32_t)layer * tensor_elements, n, d);
  }
}

// ----------------------------------------------------------------------------
// Transformer forward pass — the core of llama2.c

float *llama_forward(int token, int pos) {
  LlamaConfig *p = &g_config;
  LlamaWeights *w = &g_weights;
  LlamaState *s = &g_state;
  float *x = s->x;
  int dim = p->dim;
  int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
  int kv_mul = p->n_heads / p->n_kv_heads;
  int hidden_dim = p->hidden_dim;
  int head_size = dim / p->n_heads;

  // Copy token embedding into x
  float *content_row = w->token_embedding_table + token * dim;
  memcpy(x, content_row, dim * sizeof(float));

  // Forward through all transformer layers
  for (int l = 0; l < p->n_layers; l++) {

    // --- Attention RMSNorm ---
    rmsnorm(s->xb, x, w->rms_att_weight + l * dim, dim);

    // KV cache pointers for this layer and position
    int loff = l * p->seq_len * kv_dim;
    float *key_cache_row = s->key_cache + loff + pos * kv_dim;
    float *value_cache_row = s->value_cache + loff + pos * kv_dim;

    // QKV matmuls (Q8-aware)
    do_matmul(s->q, s->xb, w->wq, dim, dim, l, dim * dim);
    do_matmul(key_cache_row, s->xb, w->wk, dim, kv_dim, l, dim * kv_dim);
    do_matmul(value_cache_row, s->xb, w->wv, dim, kv_dim, l, dim * kv_dim);

    // --- RoPE (Rotary Positional Encoding) ---
    for (int i = 0; i < dim; i += 2) {
      int head_dim = i % head_size;
      float freq =
          1.0f / powf_float(10000.0f, (float)head_dim / (float)head_size);
      float val = (float)pos * freq;
      float fcr = cos_float(val);
      float fci = sin_float(val);
      int rotn = i < kv_dim ? 2 : 1;
      for (int v = 0; v < rotn; v++) {
        float *vec = v == 0 ? s->q : key_cache_row;
        float v0 = vec[i];
        float v1 = vec[i + 1];
        vec[i] = v0 * fcr - v1 * fci;
        vec[i + 1] = v0 * fci + v1 * fcr;
      }
    }

    // --- Multi-Head Attention ---
    for (int h = 0; h < p->n_heads; h++) {
      float *q = s->q + h * head_size;
      float *att = s->att + h * p->seq_len;

      // Compute attention scores
      for (int t = 0; t <= pos; t++) {
        float *k = s->key_cache + loff + t * kv_dim + (h / kv_mul) * head_size;
        float score = 0.0f;
        for (int i = 0; i < head_size; i++) {
          score += q[i] * k[i];
        }
        score /= sqrt_float((float)head_size);
        att[t] = score;
      }

      // Softmax the attention scores
      softmax(att, pos + 1);

      // Weighted sum of values
      float *xb = s->xb + h * head_size;
      memset(xb, 0, head_size * sizeof(float));
      for (int t = 0; t <= pos; t++) {
        float *v =
            s->value_cache + loff + t * kv_dim + (h / kv_mul) * head_size;
        float a = att[t];
        for (int i = 0; i < head_size; i++) {
          xb[i] += a * v[i];
        }
      }
    }

    // Output projection (Q8-aware)
    do_matmul(s->xb2, s->xb, w->wo, dim, dim, l, dim * dim);

    // Residual connection
    for (int i = 0; i < dim; i++)
      x[i] += s->xb2[i];

    // --- FFN RMSNorm ---
    rmsnorm(s->xb, x, w->rms_ffn_weight + l * dim, dim);

    // FFN: SwiGLU (Q8-aware)
    do_matmul(s->hb, s->xb, w->w1, dim, hidden_dim, l, dim * hidden_dim);
    do_matmul(s->hb2, s->xb, w->w3, dim, hidden_dim, l, dim * hidden_dim);

    // SiLU(w1) * w3
    for (int i = 0; i < hidden_dim; i++) {
      float val = s->hb[i];
      val *= (1.0f / (1.0f + exp_float(-val)));
      val *= s->hb2[i];
      s->hb[i] = val;
    }

    // Output of FFN (Q8-aware)
    do_matmul(s->xb, s->hb, w->w2, hidden_dim, dim, l, hidden_dim * dim);

    // Residual connection
    for (int i = 0; i < dim; i++)
      x[i] += s->xb[i];
  }

  // Final RMSNorm
  rmsnorm(x, x, w->rms_final_weight, dim);

  // Classifier into logits (Q8-aware for non-shared weights)
  if (g_quantized && w->wcls != w->token_embedding_table) {
    matmul_q8(s->logits, x, w->wcls, p->dim, p->vocab_size);
  } else {
    matmul(s->logits, x, w->wcls, p->dim, p->vocab_size);
  }

  return s->logits;
}

// ----------------------------------------------------------------------------
// Argmax sampling

static int sample_argmax(float *logits, int vocab_size) {
  int best = 0;
  float best_val = logits[0];
  for (int i = 1; i < vocab_size; i++) {
    if (logits[i] > best_val) {
      best_val = logits[i];
      best = i;
    }
  }
  return best;
}

// ----------------------------------------------------------------------------
// Load model from disk (Karpathy's .bin format)
// Layout: [Config (28 bytes)] [weights (float32)]

int llama_load_model(uint32_t disk_start_sector) {
  print_string("[LLM] Loading Llama2 model from disk...\n", 0x0E);

  // Step 1: Read the config header (28 bytes = 1 sector is enough)
  void *hdr_buf = mem_alloc(512);
  read_sectors_ATA_PIO((uint32_t)hdr_buf, disk_start_sector, 1);

  // Parse config
  int *config_data = (int *)hdr_buf;
  g_config.dim = config_data[0];
  g_config.hidden_dim = config_data[1];
  g_config.n_layers = config_data[2];
  g_config.n_heads = config_data[3];
  g_config.n_kv_heads = config_data[4];
  int raw_vocab = config_data[5];
  g_config.seq_len = config_data[6];

  int shared_weights = raw_vocab > 0 ? 1 : 0;
  g_config.vocab_size = abs_int(raw_vocab);

  // Print model config
  print_string("[LLM] Config:\n", 0x0B);
  print_string("  dim=", 0x0F);
  print_number(g_config.dim, 0x0E);
  print_string(" hidden=", 0x0F);
  print_number(g_config.hidden_dim, 0x0E);
  print_string(" layers=", 0x0F);
  print_number(g_config.n_layers, 0x0E);
  print_string(" heads=", 0x0F);
  print_number(g_config.n_heads, 0x0E);
  print_string("\n", 0x07);
  print_string("  kv_heads=", 0x0F);
  print_number(g_config.n_kv_heads, 0x0E);
  print_string(" vocab=", 0x0F);
  print_number(g_config.vocab_size, 0x0E);
  print_string(" seq_len=", 0x0F);
  print_number(g_config.seq_len, 0x0E);
  print_string("\n", 0x07);

  // Step 2: Calculate total model size
  int head_size = g_config.dim / g_config.n_heads;
  int kv_dim = (g_config.dim * g_config.n_kv_heads) / g_config.n_heads;
  uint32_t n_params = 0;
  n_params += g_config.vocab_size * g_config.dim; // token embeddings
  n_params += g_config.n_layers * g_config.dim;   // rms_att
  n_params += g_config.n_layers * g_config.dim * g_config.dim;        // wq
  n_params += g_config.n_layers * g_config.dim * kv_dim;              // wk
  n_params += g_config.n_layers * g_config.dim * kv_dim;              // wv
  n_params += g_config.n_layers * g_config.dim * g_config.dim;        // wo
  n_params += g_config.n_layers * g_config.dim;                       // rms_ffn
  n_params += g_config.n_layers * g_config.dim * g_config.hidden_dim; // w1
  n_params += g_config.n_layers * g_config.hidden_dim * g_config.dim; // w2
  n_params += g_config.n_layers * g_config.dim * g_config.hidden_dim; // w3
  n_params += g_config.dim;                     // rms_final
  n_params += g_config.seq_len * head_size / 2; // freq_cis_real
  n_params += g_config.seq_len * head_size / 2; // freq_cis_imag
  if (!shared_weights)
    n_params += g_config.vocab_size * g_config.dim; // wcls

  uint32_t weight_bytes = n_params * sizeof(float);
  uint32_t total_bytes = 28 + weight_bytes; // config + weights
  uint32_t total_sectors = (total_bytes + 511) / 512;

  print_string("  params=", 0x0F);
  print_number(n_params / 1000, 0x0E);
  print_string("K weight_size=", 0x0F);
  print_number(weight_bytes / 1024, 0x0E);
  print_string("KB sectors=", 0x0F);
  print_number(total_sectors, 0x0E);
  print_string("\n", 0x07);

  // Step 3: Allocate memory and load all data from disk
  print_string("[LLM] Loading weights from disk (this may take a moment)...\n",
               0x0E);
  float *model_data = (float *)mem_alloc(total_bytes);
  if (!model_data) {
    print_string("[LLM] ERROR: Not enough memory!\n", 0x0C);
    return 0;
  }

  // Load entire model from disk
  read_disk_large((uint32_t)model_data, disk_start_sector, total_sectors);

  // Step 4: Map weight pointers (skip the 28-byte config header)
  float *weights_start = (float *)((uint8_t *)model_data + 28);
  map_weights(&g_weights, &g_config, weights_start, shared_weights);

  print_string("[LLM] Weights mapped into memory.\n", 0x0A);

  // Step 5: Allocate inference buffers
  print_string("[LLM] Allocating inference buffers...\n", 0x0E);
  alloc_state(&g_state, &g_config);

  // Memory usage report
  uint32_t heap_used = get_heap_usage() - 0x1000000;
  print_string("[LLM] Total memory used: ", 0x0A);
  print_number(heap_used / 1024, 0x0E);
  print_string("KB\n", 0x0A);

  g_model_loaded = 1;
  g_quantized = 0;
  print_string("[LLM] Llama2 model ready! (float32)\n", 0x0A);
  return 1;
}

// ----------------------------------------------------------------------------
// Load Q8-quantized model from disk
// Format: [Config (28 bytes)] [embedding float32] [rms_att float32] [wq Q8] ...

int llama_load_model_q8(uint32_t disk_start_sector) {
  print_string("[LLM-Q8] Loading quantized model from disk...\n", 0x0E);

  // Step 1: Read config (same format as float32)
  void *hdr_buf = mem_alloc(512);
  read_sectors_ATA_PIO((uint32_t)hdr_buf, disk_start_sector, 1);

  int *config_data = (int *)hdr_buf;
  g_config.dim = config_data[0];
  g_config.hidden_dim = config_data[1];
  g_config.n_layers = config_data[2];
  g_config.n_heads = config_data[3];
  g_config.n_kv_heads = config_data[4];
  int raw_vocab = config_data[5];
  g_config.seq_len = config_data[6];

  int shared_weights = raw_vocab > 0 ? 1 : 0;
  g_config.vocab_size = abs_int(raw_vocab);

  int head_size = g_config.dim / g_config.n_heads;
  int kv_dim = (g_config.dim * g_config.n_kv_heads) / g_config.n_heads;
  int nl = g_config.n_layers;
  int dim = g_config.dim;

  print_string("[LLM-Q8] Config:\n", 0x0B);
  print_string("  dim=", 0x0F);
  print_number(dim, 0x0E);
  print_string(" hidden=", 0x0F);
  print_number(g_config.hidden_dim, 0x0E);
  print_string(" layers=", 0x0F);
  print_number(nl, 0x0E);
  print_string(" heads=", 0x0F);
  print_number(g_config.n_heads, 0x0E);
  print_string(" vocab=", 0x0F);
  print_number(g_config.vocab_size, 0x0E);
  print_string("\n", 0x07);

  // Step 2: Calculate Q8 data size
  uint32_t data_size = 0;
  // float32: embedding + rms norms
  data_size += g_config.vocab_size * dim * 4; // token_embedding
  data_size += nl * dim * 4;                  // rms_att_weight
  data_size += nl * dim * 4;                  // rms_ffn_weight
  data_size += dim * 4;                       // rms_final_weight
  // Q8: big matmul weights
  data_size += nl * q8_size(dim * (g_config.n_heads * head_size)); // wq
  data_size += nl * q8_size(dim * kv_dim);                         // wk
  data_size += nl * q8_size(dim * kv_dim);                         // wv
  data_size += nl * q8_size((g_config.n_heads * head_size) * dim); // wo
  data_size += nl * q8_size(dim * g_config.hidden_dim);            // w1
  data_size += nl * q8_size(g_config.hidden_dim * dim);            // w2
  data_size += nl * q8_size(dim * g_config.hidden_dim);            // w3
  if (!shared_weights)
    data_size += q8_size(g_config.vocab_size * dim); // wcls

  uint32_t total_bytes = 28 + data_size;
  uint32_t total_sectors = (total_bytes + 511) / 512;

  print_string("  Q8 data=", 0x0F);
  print_number(data_size / 1024, 0x0E);
  print_string("KB sectors=", 0x0F);
  print_number(total_sectors, 0x0E);
  print_string("\n", 0x07);

  // Step 3: Allocate and load
  print_string("[LLM-Q8] Loading Q8 weights from disk...\n", 0x0E);
  uint8_t *model_data = (uint8_t *)mem_alloc(total_bytes);
  if (!model_data) {
    print_string("[LLM-Q8] ERROR: Not enough memory!\n", 0x0C);
    return 0;
  }
  read_disk_large((uint32_t)model_data, disk_start_sector, total_sectors);

  // Step 4: Map Q8 weight pointers
  uint8_t *weights_start = model_data + 28;
  map_weights_q8(&g_weights, &g_config, weights_start, shared_weights);
  print_string("[LLM-Q8] Q8 weights mapped.\n", 0x0A);

  // Step 5: Allocate inference buffers
  print_string("[LLM-Q8] Allocating inference buffers...\n", 0x0E);
  alloc_state(&g_state, &g_config);

  uint32_t heap_used = get_heap_usage() - 0x1000000;
  print_string("[LLM-Q8] Total memory used: ", 0x0A);
  print_number(heap_used / 1024, 0x0E);
  print_string("KB\n", 0x0A);

  g_model_loaded = 1;
  g_quantized = 1;
  print_string("[LLM-Q8] Llama2 Q8 model ready!\n", 0x0A);
  return 1;
}

// ----------------------------------------------------------------------------
// Load tokenizer from disk (Karpathy's tokenizer.bin format)
// Layout: [max_token_length (int)] then for each token: [score (float)] [len
// (int)] [string (len bytes)]

int llama_load_tokenizer(uint32_t disk_start_sector, uint32_t size_bytes) {
  print_string("[LLM] Loading tokenizer...\n", 0x0E);

  uint32_t total_sectors = (size_bytes + 511) / 512;
  uint8_t *tok_data = (uint8_t *)mem_alloc(size_bytes);
  read_disk_large((uint32_t)tok_data, disk_start_sector, total_sectors);

  uint8_t *ptr = tok_data;

  // Read max_token_length (skip, we don't need it)
  ptr += 4;

  // Allocate vocab arrays
  int vs = g_config.vocab_size;
  g_vocab = (char **)mem_alloc(vs * sizeof(char *));
  g_vocab_scores = (float *)mem_alloc(vs * sizeof(float));

  for (int i = 0; i < vs; i++) {
    // Read score (float)
    float *score_ptr = (float *)ptr;
    g_vocab_scores[i] = *score_ptr;
    ptr += 4;

    // Read string length
    int *len_ptr = (int *)ptr;
    int len = *len_ptr;
    ptr += 4;

    // Read string
    char *token_str = (char *)mem_alloc(len + 1);
    memcpy(token_str, ptr, len);
    token_str[len] = '\0';
    ptr += len;

    g_vocab[i] = token_str;
  }

  print_string("[LLM] Tokenizer loaded: ", 0x0A);
  print_number(vs, 0x0E);
  print_string(" tokens\n", 0x0A);
  return 1;
}

// ----------------------------------------------------------------------------
// Decode: token ID -> string
static char *llama_decode(int prev_token, int token) {
  if (!g_vocab || token < 0 || token >= g_config.vocab_size)
    return "?";
  char *piece = g_vocab[token];
  // After BOS token (1), strip leading whitespace
  if (prev_token == 1 && piece[0] == ' ')
    piece++;
  return piece;
}

// ----------------------------------------------------------------------------
// Simple byte-level encoder: each ASCII char -> token ID
// In tok512: tokens 0=<unk>, 1=<s>, 2=</s>, 3-258=single bytes
// So char 'A'(65) -> token 65+3 = 68

static int encode_byte(char c) {
  int id = (unsigned char)c + 3;
  if (id >= g_config.vocab_size)
    id = 0; // <unk> if out of range
  return id;
}

// Simple string length
static int str_len(const char *s) {
  int len = 0;
  while (s[len])
    len++;
  return len;
}

// XorShift32 RNG for sampling
static uint32_t rng_state = 12345;
static uint32_t xorshift32() {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 17;
  rng_state ^= rng_state << 5;
  return rng_state;
}

// Temperature sampling: apply temperature then sample from distribution
static int sample_temperature(float *logits, int vocab_size,
                              float temperature) {
  if (temperature <= 0.01f) {
    return sample_argmax(logits, vocab_size);
  }
  // Apply temperature
  for (int i = 0; i < vocab_size; i++) {
    logits[i] /= temperature;
  }
  // Softmax
  softmax(logits, vocab_size);

  // Random sample from distribution
  float r = (float)(xorshift32() % 10000) / 10000.0f;
  float cdf = 0.0f;
  for (int i = 0; i < vocab_size; i++) {
    cdf += logits[i];
    if (r < cdf)
      return i;
  }
  return vocab_size - 1;
}

// ----------------------------------------------------------------------------
// Clear KV cache (needed between separate generations)
static void clear_kv_cache() {
  int kv_dim = (g_config.dim * g_config.n_kv_heads) / g_config.n_heads;
  int cache_size = g_config.n_layers * g_config.seq_len * kv_dim;
  memset(g_state.key_cache, 0, cache_size * sizeof(float));
  memset(g_state.value_cache, 0, cache_size * sizeof(float));
}

// ----------------------------------------------------------------------------
// Generate text (no prompt, starts from BOS)

void llama_generate(int max_tokens, float temperature) {
  if (!g_model_loaded) {
    print_string("[LLM] Error: model not loaded!\n", 0x0C);
    return;
  }

  clear_kv_cache();
  print_string("AI > ", 0x0D);

  int token = 1; // BOS token
  int prev_token = 0;
  int pos = 0;

  if (max_tokens > g_config.seq_len)
    max_tokens = g_config.seq_len;

  for (int step = 0; step < max_tokens; step++) {
    float *logits = llama_forward(token, pos);
    int next = sample_temperature(logits, g_config.vocab_size, temperature);

    if (next == 2)
      break; // EOS

    if (step > 0) {
      char *piece = llama_decode(prev_token, next);
      print_string(piece, 0x0F);
    }

    prev_token = token;
    token = next;
    pos++;
  }

  print_string("\n", 0x07);
}

// ----------------------------------------------------------------------------
// Generate text with a user prompt
// Encodes the prompt as byte-level tokens, feeds them through the model,
// then continues generating autoregressively.

void llama_generate_with_prompt(const char *prompt, int max_tokens,
                                float temperature) {
  if (!g_model_loaded) {
    print_string("[LLM] Error: model not loaded!\n", 0x0C);
    return;
  }

  clear_kv_cache();

  int prompt_len = str_len(prompt);
  if (max_tokens > g_config.seq_len)
    max_tokens = g_config.seq_len;

  print_string("AI > ", 0x0D);

  // Phase 1: Process BOS token
  int token = 1; // BOS
  int prev_token = 0;
  int pos = 0;
  float *logits = llama_forward(token, pos);
  prev_token = token;
  pos++;

  // Phase 2: Feed prompt tokens (don't sample, force the prompt)
  for (int i = 0; i < prompt_len && pos < max_tokens; i++) {
    token = encode_byte(prompt[i]);
    logits = llama_forward(token, pos);

    // Print the prompt character
    char ch[2] = {prompt[i], '\0'};
    print_string(ch, 0x0E); // Yellow = user prompt

    prev_token = token;
    pos++;
  }

  print_string(" ", 0x0F); // Space after prompt

  // Phase 3: Generate continuation (autoregressive)
  token = sample_temperature(logits, g_config.vocab_size, temperature);

  for (int step = 0; step < max_tokens - pos; step++) {
    if (token == 2)
      break; // EOS

    char *piece = llama_decode(prev_token, token);
    print_string(piece, 0x0F); // White = AI generated

    logits = llama_forward(token, pos);
    prev_token = token;
    token = sample_temperature(logits, g_config.vocab_size, temperature);
    pos++;
  }

  print_string("\n", 0x07);
}

// Quick status check
int llama_is_loaded() { return g_model_loaded; }

int llama_get_vocab_size() { return g_model_loaded ? g_config.vocab_size : 0; }
