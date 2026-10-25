#!/usr/bin/env bash
# Chat with Llama 3.2 1B (F16) right in the terminal.
exec "$HOME/llama.cpp/build/bin/llama-cli" -m "$HOME/models/Llama-3.2-1B-Instruct-f16.gguf" \
  -c 131072 -t "$(nproc)" -fa on "$@"
