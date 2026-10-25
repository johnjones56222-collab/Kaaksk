#!/usr/bin/env bash
# A quick tunnel can expire on Cloudflare's side ("Tunnel not found") while
# cloudflared keeps running and retrying, so start.sh / node-start.sh never
# notice. Every minute, ask the public URL for /health through Cloudflare;
# after 3 failures in a row, kill cloudflared so the supervisor loop starts a
# fresh tunnel and re-registers it. 200/401/503 all mean the tunnel is alive
# (the hub answers 401, a node 200, a node still loading 503).
fails=0
while true; do
  sleep 60
  url=$(cat "$HOME/tunnel.url" 2>/dev/null)
  [ -n "$url" ] || continue
  code=$(curl -s -o /dev/null -w '%{http_code}' -m 20 "$url/health")
  case "$code" in
    200|401|503) fails=0 ;;
    *) fails=$((fails + 1)); echo "$(date -u +%FT%TZ) $url/health -> $code ($fails)" >> "$HOME/watchdog.log" ;;
  esac
  if [ "$fails" -ge 3 ]; then
    echo "$(date -u +%FT%TZ) restarting cloudflared" >> "$HOME/watchdog.log"
    pkill -f "cloudflared tunne[l]"
    fails=0
    sleep 60 # give the supervisor time to bring up and register a new tunnel
  fi
done
