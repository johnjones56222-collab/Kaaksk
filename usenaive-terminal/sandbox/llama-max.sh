#!/usr/bin/env bash
# Llama 3.2 1B Instruct, "max" preset: full-precision F16 weights (no
# quantization), the model's full 128K context in each of 4 parallel slots,
# all but 2 CPU cores, large batches. MODEL_FILE / ALIAS / LLM_PORT pick
# another model. The spare cores and lower priority keep the web server and
# cloudflared responsive while it generates (a starved tunnel drops).
MODEL="$HOME/models/${MODEL_FILE:-Llama-3.2-1B-Instruct-f16.gguf}"
THREADS=$(( $(nproc) > 4 ? $(nproc) - 2 : $(nproc) ))
exec nice -n 10 "$HOME/llama.cpp/build/bin/llama-server" \
  --model "$MODEL" \
  --alias "${ALIAS:-llama-3.2-1b-instruct-f16}" \
  --host 127.0.0.1 --port "${LLM_PORT:-8080}" \
  --ctx-size 524288 --parallel 4 \
  --threads "$THREADS" --threads-batch "$THREADS" \
  --batch-size 4096 --ubatch-size 1024 \
  --flash-attn on \
  --jinja
