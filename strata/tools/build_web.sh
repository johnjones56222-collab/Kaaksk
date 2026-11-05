#!/usr/bin/env bash
# Builds the browser version of Strata (phones, tablets and desktop browsers)
# into <build dir>/dist: the single-threaded build, the multi-threaded build
# and the page around them. The page picks the threaded build when the site is
# cross-origin isolated and falls back to the other one everywhere else.
#
# Usage: tools/build_web.sh [build dir]     (default: build-web)
# Needs Emscripten (emcmake and em++ on PATH, e.g. from emsdk_env.sh).
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="$(mkdir -p "${1:-$root/build-web}" && cd "${1:-$root/build-web}" && pwd)"

if ! command -v emcmake > /dev/null; then
  echo "emcmake not found: install Emscripten (https://emscripten.org) and source emsdk_env.sh" >&2
  exit 1
fi
generator=()
if command -v ninja > /dev/null; then generator=(-G Ninja); fi

for variant in single multi; do
  threads=OFF
  [ "$variant" = multi ] && threads=ON
  emcmake cmake -S "$root" -B "$out/$variant" "${generator[@]}" -DCMAKE_BUILD_TYPE=Release -DSTRATA_WEB_THREADS=$threads
  cmake --build "$out/$variant" --parallel
done

dist="$out/dist"
rm -rf "$dist"
mkdir -p "$dist"
cp "$root"/web/{index.html,manifest.webmanifest,sw.js,icon.svg,icon-192.png,icon-512.png,icon-maskable-512.png} "$dist/"
cp "$out/single/strata.js" "$out/single/strata.wasm" "$dist/"
cp "$out/multi/strata-mt.js" "$out/multi/strata-mt.wasm" "$dist/"
# Emscripten before 3.1.58 writes a separate worker script for threads.
if [ -f "$out/multi/strata-mt.worker.js" ]; then cp "$out/multi/strata-mt.worker.js" "$dist/"; fi

echo
echo "Web build ready: $dist"
echo "Try it locally:  python3 $root/tools/serve_web.py $dist"
echo "Deploy: copy the folder to any static web host (https is needed for installing it as an app)."
