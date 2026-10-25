# usenaive-terminal

A website served by a Usenaive computer set to the plan's maximum size
(16 vCPU, 64 GB RAM, 100 GB disk, Ubuntu 24.04):
https://usenaive-terminal.johnjones56222.workers.dev

- **Terminal**: a real shell on the computer (xterm.js → WebSocket → node-pty →
  tmux session `main`). You can run anything, and programs keep running after
  you close the page. The user has passwordless `sudo`.
- **Llama Chat**: the "Llama 3.2 1B Instruct (Max)" preset. One button starts
  `llama-server` (llama.cpp built with `GGML_NATIVE`) with full-precision F16
  weights (no quantization), the model's full 131,072-token context in each of
  4 parallel slots, all 16 threads, and batch 4096 / ubatch 1024. A chat window
  then streams replies (about 35 tok/s), with a system prompt, temperature and
  max tokens. The chat is kept in the browser.
- **System**: CPU, RAM, disk, load and uptime.
- The same models work from the terminal: `llama-max` runs the server preset
  in the foreground and `llama-chat` chats with the model in the terminal.

The whole site sits behind one password (`WEB_PASSWORD`). Anyone with it gets
a full shell on the computer, so share it carefully.

## How it fits together

```
browser ──> Worker (usenaive-terminal.johnjones56222.workers.dev)
              │  proxies HTTP, SSE and WebSockets to the URL in KV
              ▼
           Cloudflare quick tunnel (*.trycloudflare.com)
              ▼
           Usenaive computer: sandbox/server.js on 127.0.0.1:7860
              ├─ /            page (sandbox/public/index.html)
              ├─ /ws/term     tmux shell
              ├─ /api/llama/* start / stop / status of llama-server
              └─ /llm/*       proxy to llama-server on 127.0.0.1:8080
```

A quick tunnel gets a new URL every time it restarts, so `sandbox/start.sh`
POSTs the current one to the Worker's `/__register` every minute (with
`REGISTER_SECRET`), and the Worker keeps it in KV. The address you share never
changes. When the computer is paused, the Worker shows an "offline" page.

## Files

- `sandbox/server.js`: web server, terminal, llama controls and proxy.
- `sandbox/public/index.html`: the page.
- `sandbox/llama-max.sh`: the max preset (also `/usr/local/bin/llama-max`).
- `sandbox/llama-chat.sh`: terminal chat (also `/usr/local/bin/llama-chat`).
- `sandbox/start.sh`: keeps the server and tunnel running and registers the URL.
- `worker/`: the Cloudflare Worker (KV binding `KV`, secret `REGISTER_SECRET`).

## Setting up a computer

On a fresh Usenaive computer (`vcpu: 16, memory_mb: 65536, disk_gb: 100`):

```sh
sudo apt-get update && sudo apt-get install -y cmake build-essential tmux htop
git clone --depth 1 https://github.com/ggml-org/llama.cpp ~/llama.cpp
cd ~/llama.cpp && cmake -B build -DGGML_NATIVE=ON -DLLAMA_CURL=OFF -DCMAKE_BUILD_TYPE=Release \
  && cmake --build build -j16 --target llama-server llama-cli
mkdir -p ~/models && curl -fL -o ~/models/Llama-3.2-1B-Instruct-f16.gguf \
  https://huggingface.co/bartowski/Llama-3.2-1B-Instruct-GGUF/resolve/main/Llama-3.2-1B-Instruct-f16.gguf
curl -fsSL -o /tmp/cf.deb https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64.deb \
  && sudo dpkg -i /tmp/cf.deb
# copy sandbox/ to ~/usenaive-terminal, then:
cd ~/usenaive-terminal && npm install --omit=dev
sudo ln -sf ~/usenaive-terminal/llama-max.sh /usr/local/bin/llama-max
sudo ln -sf ~/usenaive-terminal/llama-chat.sh /usr/local/bin/llama-chat
cat > ~/.usenaive-terminal.env <<EOF
WEB_PASSWORD=...
WORKER_URL=https://usenaive-terminal.johnjones56222.workers.dev
REGISTER_SECRET=...   # same value as the Worker secret
EOF
chmod 600 ~/.usenaive-terminal.env
setsid nohup ./start.sh > ~/start.log 2>&1 < /dev/null &
```

After the computer restarts or resumes, run the last line again. Logs:
`~/web.log`, `~/tunnel.log`, `~/llama.log`.
