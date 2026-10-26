#!/usr/bin/env bash
# A quick tunnel can expire on Cloudflare's side ("Tunnel not found") while
# cloudflared keeps running and retrying, so start.sh / node-start.sh never
# notice. Every 20 seconds, ask the public URL for /health through Cloudflare;
# after 3 failures in a row (~1 minute), kill cloudflared so the supervisor
# loop starts a fresh tunnel and re-registers it. When Cloudflare deletes a
# quick tunnel its hostname stops resolving, so curl reports 000. 200/401/503
# all mean the tunnel is alive (the hub answers 401, a node 200, a node still
# loading 503).
# Usenaive can suspend a computer for hours; its tunnel is gone by the time it
# wakes, so a jump in the clock between checks restarts cloudflared at once.
fails=0
last=$(date +%s)
while true; do
  sleep 20
  now=$(date +%s)
  gap=$((now - last))
  last=$now
  url=$(cat "$HOME/tunnel.url" 2>/dev/null)
  [ -n "$url" ] || continue
  if [ "$gap" -gt 120 ]; then
    echo "$(date -u +%FT%TZ) woke after ${gap}s" >> "$HOME/watchdog.log"
    code=suspended
  else
    code=$(curl -s -o /dev/null -w '%{http_code}' -m 20 "$url/health")
  fi
  case "$code" in
    suspended) fails=3 ;;
    200|401|503) fails=0 ;;
    *) fails=$((fails + 1)); echo "$(date -u +%FT%TZ) $url/health -> $code ($fails)" >> "$HOME/watchdog.log" ;;
  esac
  if [ "$fails" -ge 3 ]; then
    echo "$(date -u +%FT%TZ) restarting cloudflared" >> "$HOME/watchdog.log"
    pkill -f "cloudflared tunne[l]"
    fails=0
    sleep 45 # give the supervisor time to bring up and register a new tunnel
    last=$(date +%s)
  fi
done
