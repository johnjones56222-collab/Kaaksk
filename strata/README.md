# Strata

An endless voxel sandbox written from scratch in C++17. You can dig, build and explore a procedurally generated
world of mountains, rivers, forests, deserts, caves and oceans. It is rendered with physically based lighting:
cascaded sun shadows, a scattering sky, reflective water, volumetric clouds, bloom and eye adaptation.

All of its art is generated in code: block textures, the pixel font and the icons. It runs on Windows, macOS and
Linux (OpenGL 3.3). In a web browser it runs on phones, tablets and computers (WebGL 2), with touch controls.

## Features

**World**
- Infinite terrain streamed in 16×256×16 columns around the player. Generation, lighting and meshing run on worker
  threads. Results are versioned, so edits are never lost to a stale job.
- Terrain is shaped by continents, ridged mountains, river valleys and domain-warped noise. 18 biomes are chosen by
  temperature and humidity: oceans, beaches, rivers, plains, savanna, several forest types, taiga, tundra, desert,
  mountains and snowy peaks.
- 3D caves, ore veins, and trees that are consistent across chunk borders. Generation is a pure function of the
  seed, so a seed always gives the same world.
- Sky light and block light (torches, lamps, lava) spread with incremental updates when blocks change. Flowing
  water and lava, falling sand and gravel.
- Worlds are saved continuously, in compressed chunk files plus a small level file.

**Rendering**
- HDR pipeline with 2–3 shadow cascades (soft PCF, stabilised against shimmering).
- A sky from single scattering (Rayleigh, Mie, ozone) with multiple-scattering ambient light. The same model on the
  CPU drives the sun and sky light colours, so the world always matches the sky. Sun, moon phases and stars.
- Water with screen-space reflections, refraction, absorption and waves. Underwater fog, and lava glow.
- Ray-marched clouds that light up at sunset. Aerial perspective, so distant terrain fades into the haze.
- Per-vertex ambient occlusion, smooth lighting, biome tints, waving plants, bloom, automatic exposure, a filmic tone
  curve and FXAA.

**Gameplay**
- Survival: health, fall and drowning damage, lava and cacti, item drops, an inventory and crafting.
- Creative: flying, instant breaking and a palette of every block.
- A console with commands, which doubles as a scripting interface (`--exec`).

**Platforms**
- Desktop: Windows, macOS and Linux (OpenGL 3.3 core).
- Browsers on phones, tablets and computers (WebAssembly + WebGL 2):
  - Touch controls, a render-scale setting, lighter defaults on phones and saves in browser storage.
  - Installable as an app (PWA) that works offline.
  - Multi-threaded where the browser allows it.

## Building for the desktop

Requirements: CMake 3.16+, a C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+) and GLFW 3.3+. Without a system GLFW,
CMake downloads GLFW 3.4 and checks its hash.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/strata
```

On Debian/Ubuntu: `sudo apt install build-essential cmake libglfw3-dev`.
On macOS: `brew install cmake glfw`.

## Building for phones, tablets and browsers

Requirements: [Emscripten](https://emscripten.org) (3.1.6 or newer; Ubuntu's `emscripten` package works) and CMake.

```sh
tools/build_web.sh                      # builds into build-web/dist
python3 tools/serve_web.py build-web/dist
```

Open <http://localhost:8080> in a browser. To try the build on a phone, open `http://<your computer's address>:8080`
while the phone is on the same network. To publish it, copy `build-web/dist` to any static web host. HTTPS is needed
to install it as an app and to play offline.

The folder contains two builds of the game:
- `strata.js` is single-threaded and runs on every browser with WebGL 2: Chrome, Edge, Firefox, Samsung Internet,
  and Safari on iOS 15 or later. World generation runs between frames.
- `strata-mt.js` generates the world on four worker threads, which loads it several times faster. Browsers allow
  WebAssembly threads only on cross-origin isolated pages.
  - `tools/serve_web.py` sends the required headers (COOP/COEP).
  - On other hosts, the bundled service worker (`sw.js`) adds them once it is installed, so the threaded build is
    used from the second visit on.
  - `index.html` picks the right build automatically.

To build a single variant by hand: `emcmake cmake -S . -B build-web -DSTRATA_WEB_THREADS=OFF` (or `ON`), then
`cmake --build build-web`.

Page options work like the command line: `index.html?world=demo&seed=42&creative&render-distance=6`.

## Controls

### Keyboard and mouse

| Action | Keys |
| --- | --- |
| Move | W A S D |
| Jump / swim up / fly up | Space (double-tap in creative mode to fly) |
| Sneak / fly down | Shift |
| Sprint | Double-tap W, or hold Ctrl (desktop) |
| Break / place | Left / right mouse button |
| Pick block | Middle mouse button |
| Select hotbar slot | 1–9, mouse wheel |
| Inventory and crafting | E |
| Drop item | Q |
| Chat / command | T or / |
| Hide HUD, screenshot, debug info | F1, F2, F3 |
| Fullscreen | F11 |
| Pause | Esc |

In a browser, click the game to capture the mouse; Esc releases it and pauses.

### Touch screens

| Action | Touch |
| --- | --- |
| Move | Put your left thumb anywhere on the lower left and slide it: a stick appears under it |
| Sprint | Push the stick well past its ring while moving forward |
| Look around | Drag anywhere else |
| Place a block / use | Tap the spot |
| Break a block | Touch and hold the block (keep holding and drag to turn while breaking) |
| Jump / swim up / fly up | Arrow button, bottom right (double-tap to fly in creative mode) |
| Sneak / fly down | Down-arrow button (toggles sneaking; hold to descend while flying) |
| Fly on / off | Double-arrow button (creative mode) |
| Select a block | Tap a hotbar slot |
| Inventory, chat, pause | Buttons in the top right corner |

Text fields (world name, seed, chat) open the phone's keyboard in a dialog. The touch controls appear automatically
on touch input; a keyboard or mouse switches back. `/touch on|off` forces them.

## Settings

Settings are changed in the game (Settings menu) and saved to `strata_settings.txt` (in browser storage on the web):

| Key | Meaning |
| --- | --- |
| `render_distance` | View distance in chunks (2–32) |
| `fov`, `mouse_sensitivity`, `invert_mouse`, `touch_sensitivity` | Camera |
| `shadows` | 0 off, 1 low, 2 medium, 3 high |
| `clouds` | 0 off, 1 fast, 2 fancy |
| `reflections`, `bloom`, `antialiasing` | Effects on or off |
| `render_scale` | Resolution of the 3D view relative to the screen (0.5–1). The HUD stays sharp. |
| `brightness`, `view_bobbing`, `vsync`, `gui_scale`, `fullscreen` | Display |
| `worker_threads` | World generation threads (0 = automatic) |

The first start on a phone or tablet uses lighter defaults: render distance 6, low shadows, fast clouds and 75%
render scale.

## Commands

Open the chat with T or `/` (the speech-bubble button on touch screens) and type `/help`.

| Command | Effect |
| --- | --- |
| `tp X Y Z`, `look YAW PITCH`, `spawn`, `setspawn` | Move or turn the player, return to or set the spawn point |
| `time set T`, `time add T` | Time of day in ticks (0 sunrise, 6000 noon, 12000 sunset, 18000 midnight) |
| `gamemode creative/survival`, `fly [on/off]` | Game mode and flying |
| `give ITEM [N]`, `setblock X Y Z BLOCK`, `fill X1 Y1 Z1 X2 Y2 Z2 BLOCK` | Items and building |
| `heal`, `kill` | Health |
| `rd N`, `fov N`, `settings KEY VALUE` | Settings (same keys as the settings file, plus `fog`) |
| `select N`, `use`, `break` | Act like the player (useful in scripts) |
| `seed`, `pos`, `save`, `hud on/off`, `debug`, `fps` | Information and saving |
| `ui inventory/pause/settings/play`, `touch on/off` | Open screens and switch the control scheme |
| `screenshot [FILE]`, `wait N`, `waitchunks`, `quit` | Scripting |

## Command line

```
strata [--world NAME] [--seed N] [--creative | --survival] [--size WxH] [--fullscreen]
       [--render-distance N] [--shadows N] [--clouds N] [--render-scale X] [--no-vsync]
       [--exec "CMD; CMD"] [--frames N] [--settings FILE] [--saves DIR] [--log FILE]
```

For example, this renders a picture of a fresh world and exits:

```sh
strata --world demo --seed 7 --creative --exec "waitchunks; wait 10; screenshot demo.png; quit"
```

## Development

```sh
ctest --test-dir build --output-on-failure        # unit tests
python3 tools/check_shaders.py build/strata_shader_dump   # validates every shader as GLSL 3.30 and GLSL ES 3.00
python3 tools/check_gl_header.py /usr/include/GL/glcorearb.h   # checks the GL loader against the registry
cmake -S . -B build-asan -DSTRATA_SANITIZE=address,undefined   # sanitizer builds (also: thread)
```

The tests cover:
- maths, noise and the thread pool (threaded and inline);
- chunk storage and saving, and world-generation determinism and invariants;
- lighting rules, meshing, ray casts and the PNG writer;
- streaming with and without threads, fluids, and player physics;
- settings, and the touch controls.

`STRATA_PROFILE=1` logs the time spent in each frame.

### Layout

| Path | Contents |
| --- | --- |
| `src/core` | Maths, noise, random numbers, thread pool, logging, PNG output |
| `src/world` | Blocks, chunks, generation, lighting, meshing, ray casts, storage, world streaming |
| `src/gl` | OpenGL 3.3 loader / OpenGL ES 3.0 mapping, shader and render target helpers |
| `src/render` | Textures, atmosphere model, shaders, renderer, UI and font |
| `src/game` | Game loop and screens, player physics, input, touch controls, settings |
| `src/platform` | Window and input backends: GLFW (desktop) and HTML5 (browsers) |
| `web` | Page, PWA manifest, service worker, icons and Emscripten pre-script |
| `tools` | Web build and dev server, shader and GL header checks |
| `tests` | Unit tests |
