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
- A model picker switches between the standard model and three
  **uncensored** ("abliterated": refusal behaviour removed) ones, each on its
  own maxed-out Usenaive computer (a *model node*, 16 vCPU / 64 GB / 100 GB):

  | Model | Weights | Context | Node computer |
  |---|---|---|---|
  | Llama 3.2 1B Instruct (standard) | F16, 2.5 GB | 128K × 4 slots | the hub (this one) |
  | [huihui-ai/Llama-3.2-1B-Instruct-abliterated](https://huggingface.co/huihui-ai/Llama-3.2-1B-Instruct-abliterated) | F16, 3.0 GB | 128K × 4 slots | `llama-1b-uncensored-max` |
  | [huihui-ai/Llama-3.2-3B-Instruct-abliterated](https://huggingface.co/huihui-ai/Llama-3.2-3B-Instruct-abliterated) | F16, 7.2 GB | 128K × 3 slots | `llama-3b-uncensored-max` |
  | [mlabonne/Meta-Llama-3.1-8B-Instruct-abliterated](https://huggingface.co/mlabonne/Meta-Llama-3.1-8B-Instruct-abliterated) | BF16, 16 GB | 128K × 2 slots | `llama-8b-uncensored-max` |
  | Llama 3.1 8B Instruct + GPT-3.5 Turbo style LoRA (your fine-tune) | BF16, 16 GB + LoRA | 128K × 2 slots | `llama-8b-gpt35style-max` |
  | Llama 3.1 8B Instruct + GPT-3.5 Turbo style LoRA v2 | BF16, 16 GB + LoRA | 128K × 2 slots | `llama-8b-gpt35style-v2-max` |
  | Llama 3.1 8B Instruct + GPT-3.5 Turbo style LoRA v3 (merged) | BF16, 16 GB | 128K × 2 slots | `llama-8b-gpt35style-v3-max` |
  | [nomic-ai/gpt4all-j](https://huggingface.co/nomic-ai/gpt4all-j) v1.0 (GPT-J 6B, not Llama) | BF16 as released, FP32 compute | 2K × 1 | `gpt4all-j-v1-max` |

  The GPT-3.5 style model is the fine-tune trained on `video-gen-max`
  (`~/ft/adapter_final`: LoRA r16 / alpha 32 on q/k/v/o, 400 GPT-3.5 Turbo
  answers from `trl-lib/ultrafeedback-gpt-3.5-turbo-helpfulness`). Its node
  runs [unsloth/Llama-3.1-8B-Instruct-GGUF](https://huggingface.co/unsloth/Llama-3.1-8B-Instruct-GGUF)
  BF16 (the same base weights) with the adapter converted by llama.cpp's
  `convert_lora_to_gguf.py` and applied with `--lora` (`LORA_FILE` in the
  node's env).

  v2 (`~/ft/adapter_v2`, `train2.py`) is that adapter trained further on 718
  more answers from the same dataset (1,118 in all, 90 steps). Its node is a
  copy of the v1 node with `LORA_FILE=gpt35style-v2-lora-f32.gguf` and its
  own `NODE_NAME` / `ALIAS`.

  v3 (`llama31-gpt35style-lora-v3.tar.gz` in the model vault) is a new LoRA,
  r16 / alpha 16 on all seven projections (attention and MLP), trained with
  Unsloth on 2 GPUs over 4,394 answers (550 steps). Its node serves it merged
  into the base weights (`llama-export-lora`, which writes F16, then
  `llama-quantize ... BF16`): `MODEL_FILE=Llama-3.1-8B-Instruct-gpt35style-v3-merged-BF16.gguf`,
  no `LORA_FILE`. It streams at about 5 tok/s, half of v2: the computer
  landed on a busier host (same EPYC 9575F, ~30% slower single-core), and a
  fresh copy from its snapshot got the same.

  GPT4All-J is GPT-J, which llama.cpp can't run, so its node uses
  `model-node/hf_server.py` (`SERVER_SCRIPT` in the node's env) instead of
  llama-server: the same OpenAI-style API and MODEL_KEY auth, serving a
  CTranslate2 conversion of the v1.0 revision (commit `7e9adc0`). The v1.0
  files hold BF16 weights; CTranslate2 computes in FP32, which is exact for
  them, at about 8 tok/s (plain PyTorch managed 0.7 tok/s on these AVX2-only
  CPUs). It uses GPT4All's `### Prompt:` / `### Response:` template.

  Slots are as many full 128K contexts as fit in 64 GB next to the weights.
  The 8B streams at about 13-17 tok/s.
- **System**: CPU, RAM, disk, load and uptime.
- The same models work from the terminal: `llama-max` runs the server preset
  in the foreground and `llama-chat` chats with the model in the terminal
  (`llama-chat --uncensored` for the 1B uncensored weights, also kept here).

Other apps can use the model as an OpenAI-compatible API:
`POST https://usenaive-terminal.johnjones56222.workers.dev/llm/v1/chat/completions`
with `Authorization: Bearer <WEB_PASSWORD>` (CORS is open on `/llm/*` and
`/api/llama/*`). The `model` field picks one of `llama-3.2-1b-instruct-f16`,
`llama-3.2-1b-instruct-abliterated-f16`, `llama-3.2-3b-instruct-abliterated-f16`
`llama-3.1-8b-instruct-abliterated-bf16`, `llama-3.1-8b-instruct-gpt35style-bf16`,
`llama-3.1-8b-instruct-gpt35style-v2-bf16`, `llama-3.1-8b-instruct-gpt35style-v3-bf16`
or `gpt4all-j-v1.0`
(`GET /llm/v1/models` lists them).
The standard model loads on the first request; the nodes keep theirs loaded.
Nexus v37.46 uses this for its "SANDBOX POWERED LLM" section (`sbx/` ids).

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

Model nodes run `model-node/node-start.sh`: llama-server with `--api-key
MODEL_KEY` behind their own quick tunnel, registered with the Worker as
`node:<name>`. The hub reads the addresses from the Worker's `/__nodes`
(`?names=`, authenticated with `REGISTER_SECRET`) and proxies to them with
MODEL_KEY, so the nodes refuse anyone else.

## Files

- `sandbox/server.js`: web server, terminal, llama controls and proxy.
- `sandbox/public/index.html`: the page.
- `sandbox/llama-max.sh`: the max preset (also `/usr/local/bin/llama-max`).
- `sandbox/llama-chat.sh`: terminal chat (also `/usr/local/bin/llama-chat`).
- `sandbox/start.sh`: keeps the server and tunnel running and registers the URL.
- `sandbox/tunnel-watchdog.sh` (also in `model-node/`): started by `start.sh` /
  `node-start.sh`. A quick tunnel can die on Cloudflare's side ("Tunnel not
  found") while cloudflared keeps running; every 20 seconds the watchdog
  checks `<tunnel>/health` and after 3 failures (~1 minute) kills cloudflared
  so a fresh tunnel is started and registered. Usenaive also suspends
  computers for hours at a time, which kills their tunnels; when the clock jumps
  between checks the watchdog restarts cloudflared at once. Log: `~/watchdog.log`.
- `sandbox/usenaive-terminal.service`, `model-node/llama-node.service`:
  systemd units that start the hub / a node at boot, since Usenaive can
  restart a computer on its own.
- `worker/`: the Cloudflare Worker (KV binding `KV`, secret `REGISTER_SECRET`).
- `model-node/setup.sh`, `model-node/node-start.sh`: a model node's one-time
  setup and its server + tunnel supervisor.
- `model-node/setup-hf.sh`, `model-node/hf_server.py`: setup and server for a
  node whose model llama.cpp can't load (env: `HF_REPO`, `HF_REVISION`,
  `SERVER_SCRIPT=hf_server.py`, `CTX`, `ENGINE=ct2`).

## Setting up a computer

On a fresh Usenaive computer (`vcpu: 16, memory_mb: 65536, disk_gb: 100`):

```sh
sudo apt-get update && sudo apt-get install -y cmake build-essential tmux htop
git clone --depth 1 https://github.com/ggml-org/llama.cpp ~/llama.cpp
cd ~/llama.cpp && cmake -B build -DGGML_NATIVE=ON -DLLAMA_CURL=OFF -DCMAKE_BUILD_TYPE=Release \
  && cmake --build build -j16 --target llama-server llama-cli
mkdir -p ~/models && curl -fL -o ~/models/Llama-3.2-1B-Instruct-f16.gguf \
  https://huggingface.co/bartowski/Llama-3.2-1B-Instruct-GGUF/resolve/main/Llama-3.2-1B-Instruct-f16.gguf
curl -fL -o ~/models/Llama-3.2-1B-Instruct-abliterated-f16.gguf \
  https://huggingface.co/mradermacher/Llama-3.2-1B-Instruct-abliterated-GGUF/resolve/main/Llama-3.2-1B-Instruct-abliterated.f16.gguf
curl -fsSL -o /tmp/cf.deb https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64.deb \
  && sudo dpkg -i /tmp/cf.deb
# copy sandbox/ to ~/usenaive-terminal, then:
cd ~/usenaive-terminal && npm install --omit=dev
sudo ln -sf ~/usenaive-terminal/llama-max.sh /usr/local/bin/llama-max
sudo ln -sf ~/usenaive-terminal/llama-max-uncensored.sh /usr/local/bin/llama-max-uncensored
sudo ln -sf ~/usenaive-terminal/llama-chat.sh /usr/local/bin/llama-chat
cat > ~/.usenaive-terminal.env <<EOF
WEB_PASSWORD=...
WORKER_URL=https://usenaive-terminal.johnjones56222.workers.dev
REGISTER_SECRET=...   # same value as the Worker secret
EOF
chmod 600 ~/.usenaive-terminal.env
sudo cp usenaive-terminal.service /etc/systemd/system/
sudo systemctl daemon-reload && sudo systemctl enable --now usenaive-terminal
```

Add `MODEL_KEY=...` to the hub's env file too. The service brings everything
back after the computer restarts. Logs: `~/web.log`, `~/tunnel.log`,
`~/llama.log`.

## Setting up a model node

On another 16 vCPU / 64 GB / 100 GB computer, copy `model-node/` to `~/node`
and write `~/.llama-node.env` (mode 600):

```sh
NODE_NAME=uncensored-8b                      # matches `node` in server.js MODELS
ALIAS=llama-3.1-8b-instruct-abliterated-bf16 # the API model id
MODEL_FILE=Meta-Llama-3.1-8B-Instruct-abliterated-bf16.gguf
MODEL_URL=https://huggingface.co/mlabonne/Meta-Llama-3.1-8B-Instruct-abliterated-GGUF/resolve/main/meta-llama-3.1-8b-instruct-abliterated.bf16.gguf
PARALLEL=2                                   # 128K-token slots
# LORA_FILE=adapter.gguf                     # optional LoRA in ~/models
MODEL_KEY=...                                # same as the hub's
WORKER_URL=https://usenaive-terminal.johnjones56222.workers.dev
REGISTER_SECRET=...
```

Then `./setup.sh` once, and install the boot service:
`sudo cp llama-node.service /etc/systemd/system/ && sudo systemctl daemon-reload
&& sudo systemctl enable --now llama-node`. Logs: `~/llama.log`, `~/tunnel.log`.
