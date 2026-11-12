#!/bin/sh
# Builds the native developer tools: the headless playthrough test and the map dumper.
# Usage: tests/build_native.sh [outdir]
set -e
cd "$(dirname "$0")/.."
OUT="${1:-build-native}"
mkdir -p "$OUT"
LOGIC="src/world.cpp src/level.cpp src/level_bld.cpp src/level_yard.cpp src/level_crane.cpp src/texgen.cpp src/game.cpp src/game_events.cpp src/game_crane.cpp src/rigger.cpp src/audio.cpp src/story.cpp"
FLAGS="-std=c++17 -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -Wall -Wextra -Isrc"
g++ $FLAGS tests/headless.cpp $LOGIC -o "$OUT/headless"
g++ $FLAGS tests/worldmap.cpp src/world.cpp src/level.cpp src/level_bld.cpp src/level_yard.cpp src/level_crane.cpp src/texgen.cpp -o "$OUT/worldmap"
echo "built $OUT/headless and $OUT/worldmap"
