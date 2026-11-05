#!/data/data/com.termux/files/usr/bin/bash
# Plays Strata on an Android phone with Termux (https://termux.dev).
#
# Downloads the ready-made browser version (strata/play in the repository),
# serves it on this phone only and opens it in the browser. Run it again to
# update the game, or to restart it after Android has closed Termux. Worlds are
# kept by the browser for http://localhost:8080, so keep using the same port.
#
#   bash play_on_android.sh            update (when online), then play
#   bash play_on_android.sh --offline  play the copy downloaded last time
#   bash play_on_android.sh --stop     stop the game server
set -eu

repo="${STRATA_REPO:-johnjones56222-collab/Kaaksk}"
branch="${STRATA_BRANCH:-ccr-982b29e2-i2aaie}"
dir="${STRATA_DIR:-$HOME/strata}"
port="${STRATA_PORT:-8080}"
raw="https://raw.githubusercontent.com/$repo/$branch/strata"
files="index.html manifest.webmanifest sw.js icon.svg icon-192.png icon-512.png icon-maskable-512.png
       strata.js strata.wasm strata-mt.js strata-mt.wasm strata-mt.worker.js"

stop_server() { pkill -f "serve_web.py $dir/site" 2> /dev/null || true; }

if [ "${1:-}" = "--stop" ]; then
  stop_server
  termux-wake-unlock 2> /dev/null || true
  echo "Strata server stopped."
  exit 0
fi

command -v python > /dev/null 2>&1 || pkg install -y python
command -v curl > /dev/null 2>&1 || pkg install -y curl
mkdir -p "$dir/site"

if [ "${1:-}" != "--offline" ]; then
  echo "Downloading Strata from github.com/$repo ($branch)..."
  tmp="$dir/download"
  rm -rf "$tmp"
  mkdir -p "$tmp"
  ok=1
  for f in $files serve_web.py; do
    src="$raw/play/$f"
    [ "$f" = serve_web.py ] && src="$raw/tools/serve_web.py"
    if ! curl -fsSL --retry 3 "$src" -o "$tmp/$f"; then
      echo "  could not download $f"
      ok=0
      break
    fi
  done
  if [ "$ok" = 1 ]; then
    # Replace the old copy only once everything has arrived.
    mv "$tmp/serve_web.py" "$dir/serve_web.py"
    for f in $files; do mv "$tmp/$f" "$dir/site/$f"; done
    rm -rf "$tmp"
  elif [ -f "$dir/site/index.html" ] && [ -f "$dir/serve_web.py" ]; then
    echo "Download failed; playing the copy from last time."
  else
    echo "Download failed and there is no earlier copy. Check the internet connection and try again."
    exit 1
  fi
fi

stop_server
nohup python "$dir/serve_web.py" "$dir/site" --host 127.0.0.1 --port "$port" > "$dir/server.log" 2>&1 &
# Keep Termux, and with it the server, running while the browser is in front.
termux-wake-lock 2> /dev/null || true
for _ in 1 2 3 4 5 6 7 8 9 10; do
  if curl -fs -o /dev/null "http://127.0.0.1:$port/index.html"; then break; fi
  sleep 0.5
done
url="http://localhost:$port/"
echo "Strata is running at $url"
termux-open-url "$url" 2> /dev/null || echo "Open $url in Chrome."
