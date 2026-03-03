#!/usr/bin/env python3
"""
NeuralOS - Q8_0 Model Quantizer (No dependencies!)
Converts a Karpathy float32 .bin model to Q8_0 quantized format.
Pure Python — no numpy required.

Usage:
  python3 scripts/quantize.py stories260K.bin stories260K.q8
"""
import struct
import sys
import os

BLOCK_SIZE = 32

def quantize_block_q8(values):
    """Quantize a block of float values to Q8_0."""
    amax = max(abs(v) for v in values)
    scale = amax / 127.0 if amax > 0 else 1.0
    quantized = [max(-128, min(127, int(round(v / scale)))) for v in values]
    return scale, quantized

def quantize_tensor_q8(floats):
    """Quantize a list of floats to Q8_0 blocks."""
    n = len(floats)
    n_blocks = (n + BLOCK_SIZE - 1) // BLOCK_SIZE
    
    # Pad
    padded = floats + [0.0] * (n_blocks * BLOCK_SIZE - n)
    
    q8_data = bytearray()
    for b in range(n_blocks):
        block = padded[b * BLOCK_SIZE : (b + 1) * BLOCK_SIZE]
        scale, qs = quantize_block_q8(block)
        q8_data += struct.pack('f', scale)
        q8_data += struct.pack(f'{BLOCK_SIZE}b', *qs)
    
    return bytes(q8_data)

def read_floats(f, n):
    """Read n float32 values from file."""
    data = f.read(n * 4)
    return list(struct.unpack(f'{n}f', data))

def main():
    if len(sys.argv) < 3:
        print("Usage: python3 scripts/quantize.py <input.bin> <output.q8>")
        sys.exit(1)
    
    input_path = sys.argv[1]
    output_path = sys.argv[2]
    
    print(f"[Q8] Reading {input_path}...")
    
    with open(input_path, 'rb') as f:
        # Read config
        config_raw = f.read(28)
        dim, hidden_dim, n_layers, n_heads, n_kv_heads, vocab_size, seq_len = struct.unpack('7i', config_raw)
        shared = vocab_size > 0
        vocab_size = abs(vocab_size)
        head_size = dim // n_heads
        kv_dim = (dim * n_kv_heads) // n_heads
        
        print(f"  dim={dim} hidden={hidden_dim} layers={n_layers} heads={n_heads}")
        print(f"  kv_heads={n_kv_heads} vocab={vocab_size} seq={seq_len}")
        
        # Read tensors
        print(f"  Reading tensors...")
        token_emb = read_floats(f, vocab_size * dim)
        rms_att   = read_floats(f, n_layers * dim)
        wq        = read_floats(f, n_layers * dim * (n_heads * head_size))
        wk        = read_floats(f, n_layers * dim * kv_dim)
        wv        = read_floats(f, n_layers * dim * kv_dim)
        wo        = read_floats(f, n_layers * (n_heads * head_size) * dim)
        rms_ffn   = read_floats(f, n_layers * dim)
        w1        = read_floats(f, n_layers * dim * hidden_dim)
        w2        = read_floats(f, n_layers * hidden_dim * dim)
        w3        = read_floats(f, n_layers * dim * hidden_dim)
        rms_final = read_floats(f, dim)
        freq_real = read_floats(f, seq_len * head_size // 2)
        freq_imag = read_floats(f, seq_len * head_size // 2)
        wcls = None
        if not shared:
            wcls = read_floats(f, vocab_size * dim)
    
    # Write Q8
    print(f"\n  Quantizing...")
    raw_vocab = vocab_size if shared else -vocab_size
    
    with open(output_path, 'wb') as f:
        # Config header (28 bytes)
        f.write(struct.pack('7i', dim, hidden_dim, n_layers, n_heads, n_kv_heads, raw_vocab, seq_len))
        
        # Float32: embedding + rms_att
        f.write(struct.pack(f'{len(token_emb)}f', *token_emb))
        print(f"    token_emb: {len(token_emb)*4:,}B -> float32")
        f.write(struct.pack(f'{len(rms_att)}f', *rms_att))
        
        # Q8: attention weights
        for name, tensor in [('wq', wq), ('wk', wk), ('wv', wv), ('wo', wo)]:
            q8 = quantize_tensor_q8(tensor)
            f.write(q8)
            ratio = len(tensor) * 4 / len(q8)
            print(f"    {name}: {len(tensor)*4:,}B -> {len(q8):,}B ({ratio:.1f}x)")
        
        # Float32: rms_ffn
        f.write(struct.pack(f'{len(rms_ffn)}f', *rms_ffn))
        
        # Q8: FFN weights
        for name, tensor in [('w1', w1), ('w2', w2), ('w3', w3)]:
            q8 = quantize_tensor_q8(tensor)
            f.write(q8)
            ratio = len(tensor) * 4 / len(q8)
            print(f"    {name}: {len(tensor)*4:,}B -> {len(q8):,}B ({ratio:.1f}x)")
        
        # Float32: rms_final
        f.write(struct.pack(f'{len(rms_final)}f', *rms_final))
        
        # Q8: wcls
        if wcls:
            q8 = quantize_tensor_q8(wcls)
            f.write(q8)
            print(f"    wcls: {len(wcls)*4:,}B -> {len(q8):,}B")
    
    orig = os.path.getsize(input_path)
    q8sz = os.path.getsize(output_path)
    print(f"\n{'='*50}")
    print(f"  Original:  {orig:>10,}B ({orig/1024:.0f}KB)")
    print(f"  Q8:        {q8sz:>10,}B ({q8sz/1024:.0f}KB)")
    print(f"  Ratio:     {orig/q8sz:.2f}x compression")
    print(f"{'='*50}")

if __name__ == '__main__':
    main()
