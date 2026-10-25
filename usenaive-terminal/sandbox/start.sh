#!/usr/bin/env bash
# Keeps the web server and the Cloudflare tunnel running, and tells the Worker
# (stable URL) where the tunnel currently points. Reads ~/.usenaive-terminal.env:
#   WEB_PASSWORD, WORKER_URL, REGISTER_SECRET
cd "$(dirname "$0")"
set -a; . "$HOME/.usenaive-terminal.env"; set +a
export PORT=7860

(while true; do node server.js >> "$HOME/web.log" 2>&1; sleep 2; done) &

while true; do
  : > "$HOME/tunnel.log"
  cloudflared tunnel --no-autoupdate --url "http://127.0.0.1:$PORT" >> "$HOME/tunnel.log" 2>&1 &
  CF=$!
  URL=""
  for _ in $(seq 60); do
    URL=$(grep -o 'https://[a-z0-9-]*\.trycloudflare\.com' "$HOME/tunnel.log" | head -1)
    [ -n "$URL" ] && break
    sleep 1
  done
  if [ -n "$URL" ]; then
    echo "$URL" > "$HOME/tunnel.url"
    # Re-register every minute so the Worker always has the live URL.
    while kill -0 "$CF" 2>/dev/null; do
      curl -s -m 10 -X POST "$WORKER_URL/__register" -H "authorization: Bearer $REGISTER_SECRET" \
        -H 'content-type: application/json' -d "{\"url\":\"$URL\"}" > /dev/null
      sleep 60
    done
  fi
  kill "$CF" 2>/dev/null
  sleep 3
done
