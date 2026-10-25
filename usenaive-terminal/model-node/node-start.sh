#!/usr/bin/env bash
# Keeps this node's llama-server and Cloudflare quick tunnel running, and
# registers the tunnel URL with the Worker under NODE_NAME so the hub can
# find it. llama-server only answers requests carrying MODEL_KEY. LORA_FILE
# (optional) is a GGUF LoRA adapter in ~/models applied on top of the model.
# SERVER_SCRIPT (optional) runs that Python server instead of llama-server.
set -a; . "$HOME/.llama-node.env"; set +a
PORT=8080
# Leave 2 cores and scheduling priority to cloudflared: with every core busy
# generating, its QUIC connection timed out and the tunnel dropped (530).
export THREADS=$(( $(nproc) > 4 ? $(nproc) - 2 : $(nproc) ))

(while true; do
  if [ -n "$SERVER_SCRIPT" ]; then
    # A Transformers model llama.cpp can't load (see hf_server.py).
    PORT=$PORT nice -n 10 "$HOME/hfenv/bin/python" "$(dirname "$0")/$SERVER_SCRIPT" >> "$HOME/llama.log" 2>&1
  else
  nice -n 10 "$HOME/llama.cpp/build/bin/llama-server" \
    --model "$HOME/models/$MODEL_FILE" --alias "$ALIAS" \
    --host 127.0.0.1 --port "$PORT" --api-key "$MODEL_KEY" \
    --ctx-size $((131072 * PARALLEL)) --parallel "$PARALLEL" \
    --threads "$THREADS" --threads-batch "$THREADS" \
    --batch-size 4096 --ubatch-size 1024 \
    --flash-attn on --jinja ${LORA_FILE:+--lora "$HOME/models/$LORA_FILE"} >> "$HOME/llama.log" 2>&1
  fi
  sleep 3
done) &

# Restarts cloudflared if its tunnel dies while the process keeps running.
(cd "$(dirname "$0")" && pgrep -f "tunnel-watchdo[g]" > /dev/null || setsid ./tunnel-watchdog.sh > /dev/null 2>&1 &)

while true; do
  : > "$HOME/tunnel.log"
  cloudflared tunnel --no-autoupdate --url "http://127.0.0.1:$PORT" >> "$HOME/tunnel.log" 2>&1 &
  CF=$!
  URL=""
  for _ in $(seq 60); do
    # Quick tunnel names are hyphenated words; this skips https://api.trycloudflare.com,
    # which cloudflared logs when a tunnel request fails (e.g. at boot, before the network is up).
    URL=$(grep -oE 'https://[a-z0-9]+(-[a-z0-9]+)+\.trycloudflare\.com' "$HOME/tunnel.log" | head -1)
    [ -n "$URL" ] && break
    sleep 1
  done
  if [ -n "$URL" ]; then
    echo "$URL" > "$HOME/tunnel.url"
    while kill -0 "$CF" 2>/dev/null; do
      curl -s -m 10 -X POST "$WORKER_URL/__register" -H "authorization: Bearer $REGISTER_SECRET" \
        -H 'content-type: application/json' -d "{\"name\":\"$NODE_NAME\",\"url\":\"$URL\"}" > /dev/null
      sleep 60
    done
  fi
  kill "$CF" 2>/dev/null
  sleep 3
done
