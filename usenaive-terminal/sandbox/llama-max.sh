#!/usr/bin/env bash
# Llama 3.2 1B Instruct, "max" preset: full-precision F16 weights (no
# quantization), the model's full 128K context in each of 4 parallel slots,
# every CPU core, large batches.
MODEL="$HOME/models/Llama-3.2-1B-Instruct-f16.gguf"
exec "$HOME/llama.cpp/build/bin/llama-server" \
  --model "$MODEL" \
  --alias llama-3.2-1b-instruct-f16 \
  --host 127.0.0.1 --port "${LLM_PORT:-8080}" \
  --ctx-size 524288 --parallel 4 \
  --threads "$(nproc)" --threads-batch "$(nproc)" \
  --batch-size 4096 --ubatch-size 1024 \
  --flash-attn on \
  --jinja
