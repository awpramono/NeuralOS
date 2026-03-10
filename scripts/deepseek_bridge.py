import socket
import json
import urllib.request
import urllib.error

# Konfigurasi DeepSeek API
# PENTING: Untuk keperluan keamanan, pada level produksi API KEY harus diretas dari Environment Variable
DEEPSEEK_API_KEY = "sk-6b57599dd4454f4ab64eac7144f7a4e5"
DEEPSEEK_API_URL = "https://api.deepseek.com/chat/completions"

# Konfigurasi Bridge Server local (akan mendengarkan koneksi dari QEMU Virtual Machine)
# QEMU User Networking / SLIRP secara default akan meregister host ini di 10.0.2.2 TCP 8080
HOST = "0.0.0.0"
PORT = 8080

def query_deepseek(prompt):
    """
    Mengirim prompt text murni dari NeuralOS menuju Enskripsi HTTPS API Server DeepSeek.
    Lalu mengekstrak jawaban AI-nya kembali sebagai plain string ASCII murni.
    """
    headers = {
        "Content-Type": "application/json",
        "Authorization": f"Bearer {DEEPSEEK_API_KEY}"
    }

    is_code_gen = False
    system_prompt = "You are NeuralOS DeepSeek Bridge Backend. Keep your answer brief, concise, and straight to the point without markdown or formatting, just plain text limit up to 250 characters. Reply strictly in Indonesian language if Indonesian is used."
    if prompt.startswith("@CODE_GEN:"):
        is_code_gen = True
        prompt = prompt[10:].strip()
        system_prompt = "You are an AI code generator. Output ONLY raw C code with 'void main() { ... }'. STRICT RULES: 1. NO markdown (```c). 2. NO 'for' loops (ONLY 'while'). 3. DO NOT initialize variables during declaration ('int i = 1;' is FORBIDDEN). MUST declare first, then assign later. 4. ALL variables MUST be declared at the VERY TOP of main()! Do NOT declare global variables. Do NOT declare variables in the middle of the code. 5. Use ONLY: void print_string(char *s, char color); void print_number(int val, char color); Colors: 0x0F (white), 0x0A (green), 0x0E (yellow)."

    # Model default Deepseek-chat
    payload = {
        "model": "deepseek-chat",
        "messages": [
            {"role": "system", "content": system_prompt},
            {"role": "user", "content": prompt}
        ],
        "temperature": 0.2 if is_code_gen else 0.5,
        "max_tokens": 512 if is_code_gen else 100
    }

    req = urllib.request.Request(
        DEEPSEEK_API_URL, 
        data=json.dumps(payload).encode("utf-8"), 
        headers=headers, 
        method="POST"
    )

    try:
        with urllib.request.urlopen(req) as response:
            result_json = json.loads(response.read().decode("utf-8"))
            return result_json["choices"][0]["message"]["content"]
    except urllib.error.URLError as e:
        return f"Error connecting to DeepSeek API: {e}"
    except Exception as e:
        return f"Unexpected Bridge Error: {e}"

def start_server():
    # Menjalankan socket IPv4 UDP murni (Karena OS Bare Metal kita belum punya State-Machine TCP lengkap)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.bind((HOST, PORT))
        print(f"[*] DeepSeek Bridge Server (UDP Mode) berjalan di {HOST}:{PORT}")
        print("[*] Menunggu kiriman paket Datagram dari NeuralOS via QEMU (10.0.2.2)...\n")
        
        while True:
            data, addr = s.recvfrom(2048)
            if not data:
                continue
            
            # Mengubah bit buffer data kiriman Neural OS ke strings
            prompt = data.decode("utf-8", errors="ignore").strip()
            # Bersihkan dari header HTTP dsb bila ada typo, ambil query intinya (jika pakai double quotes)
            if '"' in prompt and not prompt.startswith("@CODE_GEN:"):
                try:
                    prompt = prompt.split('"')[1]
                except:
                    pass

            # jika ada prefix @CODE_GEN tapi pakai kutip sesudahnya
            if prompt.startswith("@CODE_GEN:") and '"' in prompt:
                try:
                    prefix = "@CODE_GEN:"
                    content = prompt[len(prefix):].split('"')[1]
                    prompt = prefix + content
                except:
                    pass

            print(f"[+] Pesan masukan (UDP) dari OS {addr}: '{prompt}'")
            
            # Memanggil Cloud API Engine sungguhan
            reply = query_deepseek(prompt)
            
            is_code_gen = prompt.startswith("@CODE_GEN:")
            
            # Mengganti karakter ganti baris dengan spasi agar pas dengan Terminal VGA AI kita yang berukuran terbatas (jika chat)
            if not is_code_gen:
                reply_clean = reply.replace('\n', ' ').replace('\r', '')
            else:
                reply_clean = reply.replace('\r', '') # Biarkan \n untuk line break code C

            # Filter backticks (Markdown code block) jika AI ngeyel
            reply_clean = reply_clean.replace("```c\n", "").replace("```c", "").replace("```C", "").replace("```\n", "").replace("```", "")

            print(f"[<] Membalas via UDP Payload: '{reply_clean}'\n")
            
            # Balas langsung ke QEMU SLIRP Address
            s.sendto(reply_clean.encode("utf-8"), addr)

if __name__ == "__main__":
    start_server()
