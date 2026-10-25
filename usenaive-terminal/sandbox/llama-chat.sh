#!/usr/bin/env bash
# Chat with Llama 3.2 1B (F16) right in the terminal. `llama-chat --uncensored`
# uses the abliterated model.
FILE=Llama-3.2-1B-Instruct-f16.gguf
if [ "$1" = "--uncensored" ]; then FILE=Llama-3.2-1B-Instruct-abliterated-f16.gguf; shift; fi
exec "$HOME/llama.cpp/build/bin/llama-cli" -m "$HOME/models/$FILE" \
  -c 131072 -t "$(nproc)" -fa on "$@"
