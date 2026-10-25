#!/usr/bin/env bash
# One-time setup of a model node: a Usenaive computer that serves a single
# Llama model for the hub. Needs ~/.llama-node.env (see README).
set -ex
set -a; . "$HOME/.llama-node.env"; set +a
sudo DEBIAN_FRONTEND=noninteractive apt-get update -y
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y cmake build-essential htop
[ -d ~/llama.cpp ] || git clone --depth 1 https://github.com/ggml-org/llama.cpp ~/llama.cpp
cd ~/llama.cpp && cmake -B build -DGGML_NATIVE=ON -DLLAMA_CURL=OFF -DCMAKE_BUILD_TYPE=Release \
  && cmake --build build -j"$(nproc)" --target llama-server llama-cli
mkdir -p ~/models
[ -s "$HOME/models/$MODEL_FILE" ] || curl -fL -o "$HOME/models/$MODEL_FILE" "$MODEL_URL"
command -v cloudflared || { curl -fsSL -o /tmp/cf.deb https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64.deb && sudo dpkg -i /tmp/cf.deb; }
echo SETUP_DONE
