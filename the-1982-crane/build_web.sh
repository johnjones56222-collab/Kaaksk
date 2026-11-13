#!/bin/sh
# Builds The 1982 Crane for the browser with Emscripten.
#   ./build_web.sh            -> dist/the-1982-crane.html (single self-contained file)
#   ./build_web.sh debug      -> build-debug/index.html (+ .js/.wasm, assertions on)
#   ./build_web.sh artifact   -> build-artifact/the-1982-crane.html (WebAssembly + JavaScript fallback in one page)
set -e
cd "$(dirname "$0")"
EMCC="${EMCC:-em++}"
SRC="src/main.cpp src/ui.cpp src/render.cpp src/game.cpp src/game_events.cpp src/game_crane.cpp src/rigger.cpp src/audio.cpp src/story.cpp \
     src/world.cpp src/level.cpp src/level_bld.cpp src/level_yard.cpp src/level_crane.cpp src/texgen.cpp"
COMMON="-std=c++17 -Wall -Wextra -Wno-unused-parameter -fno-exceptions \
  -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sENVIRONMENT=web \
  -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=67108864 -sSTACK_SIZE=2097152 \
  -sEXPORTED_FUNCTIONS=_main,_app_audio_render,_app_gesture_query,_app_lock_changed,_app_lock_failed,_app_focus_lost,_app_state,_app_stage,_app_debug,_app_debug_load,_app_draw_calls,_app_query,_malloc,_free \
  -sEXPORTED_RUNTIME_METHODS=ccall,UTF8ToString \
  --shell-file web/shell.html"
if [ "$1" = "debug" ]; then
  mkdir -p build-debug
  $EMCC $SRC -O1 -g2 -sASSERTIONS=2 $COMMON -o build-debug/index.html
  echo "built build-debug/index.html"
elif [ "$1" = "artifact" ]; then
  # one page holding the WebAssembly build and a pure-JavaScript fallback
  mkdir -p build-artifact build-js dist
  $EMCC $SRC -O3 -sASSERTIONS=0 -sSINGLE_FILE=1 -sSINGLE_FILE_BINARY_ENCODE=0 $COMMON -o dist/the-1982-crane.html
  $EMCC $SRC -O3 -sASSERTIONS=0 -sSINGLE_FILE=1 -sSINGLE_FILE_BINARY_ENCODE=0 -sWASM=0 $COMMON -o build-js/index.html
  python3 tools/make_artifact.py dist/the-1982-crane.html build-js/index.html build-artifact/the-1982-crane.html
else
  mkdir -p dist
  $EMCC $SRC -O3 -sASSERTIONS=0 -sSINGLE_FILE=1 -sSINGLE_FILE_BINARY_ENCODE=0 $COMMON -o dist/the-1982-crane.html
  ls -la dist/the-1982-crane.html
fi
