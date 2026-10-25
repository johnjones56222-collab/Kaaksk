#!/usr/bin/env bash
# One-time setup of a model node that runs a Hugging Face Transformers model
# (for architectures llama.cpp can't load, e.g. GPT-J) with hf_server.py.
# Needs ~/.llama-node.env with HF_REPO / HF_REVISION (see README).
set -ex
set -a; . "$HOME/.llama-node.env"; set +a
sudo DEBIAN_FRONTEND=noninteractive apt-get update -y
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y htop
python3 -m venv "$HOME/hfenv"
"$HOME/hfenv/bin/pip" install -q torch --index-url https://download.pytorch.org/whl/cpu
"$HOME/hfenv/bin/pip" install -q transformers accelerate huggingface_hub ctranslate2
"$HOME/hfenv/bin/python" -c "
import os
from huggingface_hub import snapshot_download
print(snapshot_download(os.environ['HF_REPO'], revision=os.environ.get('HF_REVISION') or None,
                        local_dir=os.path.expanduser('~/models/hf')))"
# CTranslate2 copy for ENGINE=ct2 (hf_server.py's default): float32 compute is
# exact for bf16/fp16 checkpoints and far faster than PyTorch on CPU.
[ -f "$HOME/models/ct2/model.bin" ] || "$HOME/hfenv/bin/ct2-transformers-converter" --model "$HOME/models/hf" \
  --output_dir "$HOME/models/ct2" --quantization float32
command -v cloudflared || { curl -fsSL -o /tmp/cf.deb https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64.deb && sudo dpkg -i /tmp/cf.deb; }
echo SETUP_DONE
