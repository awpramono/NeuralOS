#!/usr/bin/env python3
"""
NeuralOS v3.0 - Micro-GGUF Brain Generator
Two-tensor architecture:
  - embed.weight (32x32): Embedding matrix (each token gets a unique representation)
  - proj.weight  (32x32): Projection matrix (maps embeddings to next-token logits)

This guarantees correct transitions because:
  embed[src,:] ≈ one-hot at dimension src
  logits[dst] = dot(proj[dst,:], rmsnorm(embed[src,:])) ≈ proj[dst, src]
  So we just set proj[desired_dst, src] = HIGH, everything else = LOW.
"""
import struct
import math

VOCAB_SIZE = 32
END_TOKEN  = 31

# ============================================================================
# Vocabulary (must match ai.c):
#  0: "Halo! "       1: "Saya "       2: "adalah "     3: "NeuralOS, "
#  4: "asisten "     5: "AI "         6: "Anda. "      7: "Sistem "
#  8: "memori: "     9: "CPU: "      10: "2-core "    11: "aktif. "
# 12: "512MB "      13: "RAM "       14: "tersedia. " 15: "Perintah: "
# 16: "HELP "       17: "INFO "      18: "MEM "       19: "CLEAR "
# 20: "AI. "        21: "Siap "      22: "melayani! " 23: "Neural "
# 24: "engine "     25: "berjalan "  26: "di "        27: "bare-metal. "
# 28: "Ketik "      29: "perintah "  30: "apapun. "   31: "" (END)
# ============================================================================

CHAINS = {
    # Greeting: "Halo! Saya adalah NeuralOS, asisten AI Anda."
    0:  [1, 2, 3, 4, 5, 6, END_TOKEN],
    # About AI: "Saya adalah NeuralOS, asisten AI Anda."
    1:  [2, 3, 4, 5, 6, END_TOKEN],
    # Memory: "Sistem memori: 512MB RAM tersedia."
    7:  [8, 12, 13, 14, END_TOKEN],
    # CPU: "CPU: 2-core aktif."
    9:  [10, 11, END_TOKEN],
    # Help: "Perintah: HELP INFO MEM CLEAR AI."
    15: [16, 17, 18, 19, 20, END_TOKEN],
    # Default: "Siap melayani! Ketik perintah apapun."
    21: [22, 28, 29, 30, END_TOKEN],
    # Info: "Neural engine berjalan di bare-metal. 2-core aktif."
    23: [24, 25, 26, 27, 10, 11, END_TOKEN],
}

VOCAB = [
    "Halo! ", "Saya ", "adalah ", "NeuralOS, ", "asisten ", "AI ", "Anda. ",
    "Sistem ", "memori: ", "CPU: ", "2-core ", "aktif. ", "512MB ", "RAM ",
    "tersedia. ", "Perintah: ", "HELP ", "INFO ", "MEM ", "CLEAR ", "AI. ",
    "Siap ", "melayani! ", "Neural ", "engine ", "berjalan ", "di ",
    "bare-metal. ", "Ketik ", "perintah ", "apapun. ", "[END]"
]

def build_embed_matrix():
    """Each token gets a near-one-hot embedding at its own dimension."""
    E = [[0.01] * VOCAB_SIZE for _ in range(VOCAB_SIZE)]
    for i in range(VOCAB_SIZE):
        E[i][i] = 8.0
    return E

def build_proj_matrix():
    """
    proj[dst, src] = HIGH if src->dst is a desired transition, LOW otherwise.
    After rmsnorm(embed[src,:]) ≈ one-hot at dim src,
    logits[dst] = dot(proj[dst,:], state) ≈ proj[dst, src].
    """
    P = [[-1.0] * VOCAB_SIZE for _ in range(VOCAB_SIZE)]
    
    for start, chain in CHAINS.items():
        current = start
        for nxt in chain:
            P[nxt][current] = 10.0
            current = nxt
    
    # END token row: high values at the last-before-end token of each chain
    # so after the last real word, END wins the argmax
    P[END_TOKEN] = [-1.0] * VOCAB_SIZE
    for start, chain in CHAINS.items():
        # Find the token right before END in this chain
        for idx, tok in enumerate(chain):
            if tok == END_TOKEN and idx > 0:
                last_real = chain[idx - 1]
                P[END_TOKEN][last_real] = 10.0
    
    # Resolve conflicts: if a token is both a termination point (P[END][tok]=HIGH)
    # AND a continuation point (P[next][tok]=HIGH), boost the continuation
    for start, chain in CHAINS.items():
        current = start
        for nxt in chain:
            if nxt != END_TOKEN and P[END_TOKEN][current] > 5.0:
                # This token has an END signal but also needs to continue
                P[nxt][current] = 15.0  # Boost continuation over termination
            current = nxt
    
    return P

def simulate(E, P, start_token, max_steps=10):
    """Simulate the exact inference loop from ai.c"""
    d = VOCAB_SIZE
    
    def rmsnorm(x):
        ss = sum(v*v for v in x) / len(x) + 1e-5
        return [v / math.sqrt(ss) for v in x]
    
    def matmul_vec(mat, vec):
        return [sum(mat[i][j] * vec[j] for j in range(d)) for i in range(d)]
    
    def silu(x):
        if x > 50: return x
        if x < -50: return 0.0
        return x / (1.0 + math.exp(-x))
    
    def softmax(vals):
        mx = max(vals)
        exps = [math.exp(min(v - mx, 50)) for v in vals]
        s = sum(exps)
        return [e / s for e in exps]
    
    tokens = [start_token]
    current = start_token
    for step in range(max_steps):
        # 1. Embedding lookup
        state = list(E[current])
        # 2. RMSNorm
        state = rmsnorm(state)
        # 3. Projection (matmul with P)
        logits = matmul_vec(P, state)
        # 4. SiLU
        logits = [silu(v) for v in logits]
        # 5. Softmax
        logits = softmax(logits)
        # 6. Argmax
        next_tok = logits.index(max(logits))
        tokens.append(next_tok)
        if next_tok == END_TOKEN:
            break
        current = next_tok
    return tokens

def pack_string(s):
    encoded = s.encode('utf-8')
    return struct.pack('<Q', len(encoded)) + encoded

def create_micro_gguf(E, P):
    # 1. HEADER (2 tensors now)
    magic = 0x46554747
    version = 3
    tensor_count = 2
    kv_count = 1
    header = struct.pack('<IIQQ', magic, version, tensor_count, kv_count)
    
    # 2. METADATA KV
    key_data = pack_string("general.name")
    val_type = struct.pack('<I', 8)
    val_data = pack_string("NeuralOS-TinyLLM-32tok-v3")
    kv_metadata = key_data + val_type + val_data

    # 3. TENSOR INFO (two tensors)
    # Tensor 0: embed.weight (32x32)
    t0_name = pack_string("embed.weight")
    t0_n_dims = struct.pack('<I', 2)
    t0_dims = struct.pack('<QQ', VOCAB_SIZE, VOCAB_SIZE)
    t0_type = struct.pack('<I', 0)  # float32
    t0_offset = struct.pack('<Q', 0)
    tensor0_info = t0_name + t0_n_dims + t0_dims + t0_type + t0_offset
    
    # Tensor 1: proj.weight (32x32)
    embed_bytes = VOCAB_SIZE * VOCAB_SIZE * 4  # 4096 bytes
    t1_name = pack_string("proj.weight")
    t1_n_dims = struct.pack('<I', 2)
    t1_dims = struct.pack('<QQ', VOCAB_SIZE, VOCAB_SIZE)
    t1_type = struct.pack('<I', 0)  # float32
    t1_offset = struct.pack('<Q', embed_bytes)  # after embed data
    tensor1_info = t1_name + t1_n_dims + t1_dims + t1_type + t1_offset
    
    # 4. WEIGHT DATA
    embed_data = b''
    for row in E:
        for val in row:
            embed_data += struct.pack('<f', val)
    
    proj_data = b''
    for row in P:
        for val in row:
            proj_data += struct.pack('<f', val)
    
    total = header + kv_metadata + tensor0_info + tensor1_info + embed_data + proj_data
    
    print(f"[*] GGUF Brain Size: {len(total)} bytes")
    print(f"    Header:      {len(header)} bytes")
    print(f"    Metadata:    {len(kv_metadata)} bytes")
    print(f"    Tensor Info: {len(tensor0_info + tensor1_info)} bytes")
    print(f"    Embed Data:  {len(embed_data)} bytes")
    print(f"    Proj Data:   {len(proj_data)} bytes")
    print(f"    Total Weights: {len(embed_data) + len(proj_data)} bytes")
    
    return total

def verify_all(E, P):
    print(f"\n[*] Verification (same math as ai.c):")
    labels = {0:"HELLO", 1:"ABOUT-AI", 7:"MEMORY", 9:"CPU", 15:"HELP", 21:"DEFAULT", 23:"INFO"}
    all_ok = True
    for start, desired_chain in CHAINS.items():
        actual = simulate(E, P, start)
        desired = [start] + desired_chain
        ok = "OK" if actual == desired else "FAIL"
        if ok == "FAIL": all_ok = False
        text = "".join(VOCAB[t] for t in actual)
        label = labels.get(start, f"TOK-{start}")
        print(f"  [{label:10s}] [{ok}] \"{text}\"")
    return all_ok

if __name__ == '__main__':
    E = build_embed_matrix()
    P = build_proj_matrix()
    
    success = verify_all(E, P)
    
    data = create_micro_gguf(E, P)
    
    if success:
        print("\n[+] ALL chains verified successfully!")
    else:
        print("\n[!] WARNING: Some chains failed verification!")
    
    try:
        with open('disk.img', 'r+b') as f:
            f.seek(51200)
            f.write(data)
            sectors = (len(data) + 511) // 512
            print(f"[*] Brain injected at sector 100 ({len(data)} bytes, {sectors} sectors)")
    except FileNotFoundError:
        print("[!] disk.img not found. Run 'make run' to create it first.")
