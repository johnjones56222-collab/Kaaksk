# Cold Wake

A first-person 3D game that runs in the browser. You wake from cryo-sleep
aboard the survey ship ISV Astraea: the reactor is dark, the ship's AI is
offline and the crew are still frozen. Bring the ship back to life, find out
what the signal in the planet's rings is saying, and jump home.

**To play:** open `public/index.html` in a browser. It is a single
self-contained file (no server, no downloads, works offline), so it can also be
dropped on any static host. `wrangler.jsonc` is set up for `npx wrangler deploy`.

## What is in it

- **Fourteen areas** on one deck plus the exterior hull: Cryo Bay, two spine
  corridors, Central Hub, AI Core, Crew Quarters, Med Bay, Science Lab,
  Hydroponics, Engineering, Cargo Bay, Airlock, Bridge and the dorsal hull.
- **Nine missions, 24 objectives:** restore power, reboot the AI, relight the
  fusion reactor, fix life support, fight a fire, decode the signal, go on a
  spacewalk to weld hull breaches and realign the dish, defend the ship from the
  turret, plot a jump and pull the lever.
- **Ten console puzzles:** breaker balancing, memory lattice, keypad, pipe
  routing, wire matching, jump-route planning, injector timing, compound
  mixing, signal phase matching and dish alignment.
- **Optional tasks:** 12 crew logs, 6 service panels, 5 plant specimens,
  6 crew keepsakes, a coffee machine and a jukebox.
- Datapad with mission list, deck map, logs, comms transcript, settings and
  controls. Progress autosaves after every objective.
- Everything is generated in code: geometry, textures, lighting, the ringed
  planet and nebula, particles, sound effects and music. No asset files.

## Controls

| | Keyboard and mouse | Touch |
|---|---|---|
| Move | W A S D (Shift to run) | Drag on the left half (push to the edge to run) |
| Look | Mouse (click the view first) | Drag on the right half |
| Use | E (hold for welding, valves, spraying) | USE button |
| Jump | Space | Arrow button |
| Flashlight | F | Lamp button |
| Datapad / map | Tab / M | Map button |
| Pause | Esc | Pause button |

Where the browser will not lock the pointer (some embedded frames), hold the
mouse button and drag to look.

## Development

```
npm install
npm run build      # src/ -> public/index.html (minified, one file)
npm run dev        # same, unminified with a source map
npm test           # build, then run the automated playthrough
```

`public/index.html` is a build output: edit `src/` and rebuild.

- `src/engine/` renderer and post-processing, procedural textures, materials,
  the geometry builder that bakes room lighting into vertices, particles,
  input (pointer lock, touch) and the audio synthesiser.
- `src/world/` deck plan, room shell generator, props, one file per room,
  doors, live display screens and everything outside the hull.
- `src/game/` player controller, interaction, missions, story content, logs,
  turret sequence and the ending.
- `src/ui/` HUD, menus and the puzzle minigames.

### Tests

`tests/playthrough.cjs` drives the whole story in headless Chromium through
the game's debug hooks (`window.__game.debug`): it walks up to each object,
checks the game focuses it, uses it, solves each puzzle, collects every
optional item, reaches the ending and fails on any console error. It needs
[Playwright](https://playwright.dev) (`npm i -D playwright`), which is not in
`package.json` because the game itself has no runtime dependencies.
`tests/shot.cjs` takes screenshots at chosen positions.

Add `?q=low`, `?q=medium` or `?q=high` to the URL to force a graphics preset.
