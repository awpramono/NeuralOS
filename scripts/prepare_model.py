#!/usr/bin/env python3
"""
NeuralOS - Llama2 Model Preparation Script
Downloads a tiny Llama2 model and injects it into disk.img

Supports two model sizes:
  - stories260K  (~1MB model, 512 tokens) - Fast, good for demo
  - stories15M   (~60MB model, 32000 tokens) - Better output, slow to load

Usage:
  python3 scripts/prepare_model.py           # Default: stories260K
  python3 scripts/prepare_model.py stories15M  # Larger model
"""
import os
import sys
import struct
import subprocess

# Model configurations
MODELS = {
    "stories260K": {
        "model_url":  "https://huggingface.co/karpathy/tinyllamas/resolve/main/stories260K/stories260K.bin",
        "tok_url":    "https://huggingface.co/karpathy/tinyllamas/resolve/main/stories260K/tok512.bin",
        "model_file": "stories260K.bin",
        "tok_file":   "tok512.bin",
        "tok_size":   16000,
        "description": "260K params, 512 tokens, ~1MB"
    },
    "stories15M": {
        "model_url":  "https://huggingface.co/karpathy/tinyllamas/resolve/main/stories15M.bin",
        "tok_url":    "https://raw.githubusercontent.com/karpathy/llama2.c/master/tokenizer.bin",
        "tok_url_alt": "https://huggingface.co/karpathy/tinyllamas/resolve/main/tokenizer.model",
        "model_file": "stories15M.bin",
        "tok_file":   "tokenizer.bin",
        "tok_size":   512000,
        "description": "15M params, 32000 tokens, ~60MB"
    },
    "SmolLM135M": {
        "model_url":  "https://huggingface.co/imartinez/SmolLM-135M-llama2.c/resolve/main/SmolLM-135M.bin",
        "tok_url":    "https://raw.githubusercontent.com/karpathy/llama2.c/master/tokenizer.bin",
        "model_file": "SmolLM-135M.bin",
        "tok_file":   "tokenizer.bin",
        "tok_size":   512000,
        "description": "SmolLM 135M params, 32000 tokens, ~540MB (135MB Q8)"
    }
}

DISK_IMG       = "disk.img"
MODEL_SECTOR   = 200       # sector offset for model
TOK_SECTOR     = 130000    # sector offset for tokenizer

def download_file(url, path):
    """Download a file using wget/curl with fallback."""
    if os.path.exists(path):
        size = os.path.getsize(path)
        print(f"  [*] {path} already exists ({size:,} bytes)")
        return True
    
    print(f"  [*] Downloading {path}...")
    print(f"      URL: {url}")
    
    # Try wget first
    ret = os.system(f'wget -q --show-progress -O "{path}" "{url}" 2>&1')
    if ret == 0 and os.path.exists(path) and os.path.getsize(path) > 100:
        size = os.path.getsize(path)
        print(f"  [+] Downloaded: {size:,} bytes")
        return True
    
    # Try curl
    if os.path.exists(path):
        os.remove(path)
    ret = os.system(f'curl -L -o "{path}" "{url}" 2>&1')
    if ret == 0 and os.path.exists(path) and os.path.getsize(path) > 100:
        size = os.path.getsize(path)
        print(f"  [+] Downloaded: {size:,} bytes")
        return True
    
    # Try Python urllib
    try:
        import urllib.request
        urllib.request.urlretrieve(url, path)
        size = os.path.getsize(path)
        print(f"  [+] Downloaded: {size:,} bytes")
        return True
    except Exception as e:
        print(f"  [!] All download methods failed: {e}")
        if os.path.exists(path):
            os.remove(path)
        return False

def inject_file(disk_path, file_path, sector_offset):
    """Inject a file into disk.img at the given sector offset."""
    byte_offset = sector_offset * 512
    file_size = os.path.getsize(file_path)
    sectors_needed = (file_size + 511) // 512
    
    print(f"  [*] Injecting {file_path} -> {disk_path}")
    print(f"      Sector {sector_offset}, {file_size:,} bytes ({sectors_needed} sectors)")
    
    with open(file_path, 'rb') as f:
        data = f.read()
    
    with open(disk_path, 'r+b') as f:
        f.seek(byte_offset)
        f.write(data)
    
    print(f"  [+] OK!")
    return sectors_needed

def verify_model(path):
    """Read and display model config."""
    with open(path, 'rb') as f:
        config = struct.unpack('7i', f.read(28))
    
    dim, hidden_dim, n_layers, n_heads, n_kv_heads, vocab_size, seq_len = config
    shared = vocab_size > 0
    vocab_size = abs(vocab_size)
    
    print(f"\n  === Model Config ===")
    print(f"  dim={dim} hidden={hidden_dim} layers={n_layers}")
    print(f"  heads={n_heads} kv_heads={n_kv_heads}")
    print(f"  vocab={vocab_size} seq_len={seq_len}")
    
    file_size = os.path.getsize(path)
    n_params = (file_size - 28) // 4
    print(f"  params={n_params:,} ({n_params/1e6:.2f}M)")
    print(f"  size={file_size:,} bytes ({file_size/1e6:.1f}MB)")
    return vocab_size

if __name__ == '__main__':
    # Choose model (skip flags like --q8)
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    model_name = args[0] if args else "stories260K"
    if model_name not in MODELS:
        print(f"Unknown model: {model_name}")
        print(f"Available: {', '.join(MODELS.keys())}")
        sys.exit(1)
    
    cfg = MODELS[model_name]
    
    print("=" * 60)
    print(f"  NeuralOS - Llama2 Model: {model_name}")
    print(f"  {cfg['description']}")
    print("=" * 60)
    
    # Step 1: Download model
    print(f"\n[Step 1] Download model")
    if not download_file(cfg["model_url"], cfg["model_file"]):
        print("[!] Cannot continue without model.")
        sys.exit(1)
    
    vocab_size = verify_model(cfg["model_file"])
    
    # Step 2: Download tokenizer
    print(f"\n[Step 2] Download tokenizer")
    if not download_file(cfg["tok_url"], cfg["tok_file"]):
        # Try alt URL if available
        if "tok_url_alt" in cfg:
            print(f"  [*] Trying alternative URL...")
            if not download_file(cfg["tok_url_alt"], cfg["tok_file"]):
                print("[!] Cannot continue without tokenizer. Exiting.")
                sys.exit(1)
        else:
            print("[!] Cannot continue without tokenizer. Exiting.")
            sys.exit(1)
    
    tok_size = os.path.getsize(cfg["tok_file"])
    print(f"  Tokenizer: {tok_size:,} bytes")
    
    # Step 3: Create disk image
    print(f"\n[Step 3] Prepare disk image")
    model_size = os.path.getsize(cfg["model_file"])
    min_disk_mb = max(128, (TOK_SECTOR * 512 + tok_size) // (1024*1024) + 1)
    
    if not os.path.exists(DISK_IMG) or os.path.getsize(DISK_IMG) < min_disk_mb * 1024 * 1024:
        print(f"  Creating {DISK_IMG} ({min_disk_mb}MB)...")
        os.system(f"dd if=/dev/zero of={DISK_IMG} bs=1M count={min_disk_mb} 2>/dev/null")
    
    # Step 4: Check if --q8 flag is present
    use_q8 = "--q8" in sys.argv
    
    if use_q8:
        # Quantize the model to Q8
        q8_file = cfg["model_file"].replace(".bin", ".q8")
        if not os.path.exists(q8_file):
            print(f"\n[Step 4] Quantizing to Q8...")
            ret = os.system(f'python3 scripts/quantize.py "{cfg["model_file"]}" "{q8_file}"')
            if ret != 0:
                print("[!] Quantization failed!")
                sys.exit(1)
        else:
            q8_size = os.path.getsize(q8_file)
            print(f"\n[Step 4] Q8 model exists: {q8_file} ({q8_size:,} bytes)")
        
        # Add Q8 magic marker: 'NNQ8' at bytes 508-511 of first sector
        # (last 4 bytes of the first 512-byte sector, safe padding area)
        print(f"\n[Step 5] Inject Q8 model at sector 300")
        with open(q8_file, 'rb') as f:
            q8_data = bytearray(f.read())
        # Pad first sector to 512 bytes if needed, then write magic
        while len(q8_data) < 512:
            q8_data.append(0)
        q8_data[508:512] = b'NNQ8'
        q8_tmp = q8_file + ".tmp"
        with open(q8_tmp, 'wb') as f:
            f.write(q8_data)
        inject_file(DISK_IMG, q8_tmp, 300)
        os.remove(q8_tmp)
        
        # Also inject float32 at sector 200 as fallback
        print(f"\n[Step 6] Inject float32 model at sector 200 (fallback)")
        inject_file(DISK_IMG, cfg["model_file"], MODEL_SECTOR)
    else:
        # Float32 only
        print(f"\n[Step 4] Inject float32 model")
        inject_file(DISK_IMG, cfg["model_file"], MODEL_SECTOR)
    
    # Inject tokenizer
    print(f"\n[Step {'7' if use_q8 else '5'}] Inject tokenizer")
    inject_file(DISK_IMG, cfg["tok_file"], TOK_SECTOR)
    
    # Inject 32-token GGUF brain
    print(f"\n[Step {'8' if use_q8 else '6'}] Inject 32-token GGUF")
    os.system("python3 scripts/make_gguf.py 2>/dev/null")

    print("\n" + "=" * 60)
    if use_q8:
        print("  READY! Q8 quantized model injected into disk.img")
    else:
        print("  READY! Float32 model injected into disk.img")
    print("=" * 60)
    print(f"\n  Model:     {cfg['model_file']} at sector {MODEL_SECTOR}")
    if use_q8:
        print(f"  Q8 Model:  {q8_file} at sector 300")
    print(f"  Tokenizer: {cfg['tok_file']} at sector {TOK_SECTOR}")
    print(f"\n  Type 'LLAMA' at the NeuralOS prompt to generate!")

