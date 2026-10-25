#!/usr/bin/env bash
# Same max preset with huihui-ai/Llama-3.2-1B-Instruct-abliterated (refusal
# direction removed), F16 GGUF from mradermacher, on port 8081.
MODEL_FILE=Llama-3.2-1B-Instruct-abliterated-f16.gguf \
ALIAS=llama-3.2-1b-instruct-abliterated-f16 \
LLM_PORT=8081 \
exec "$(dirname "$(readlink -f "$0")")/llama-max.sh" "$@"
