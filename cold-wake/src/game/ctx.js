// Shared handle to the running game. Filled in by main.js at boot so modules
// can reach each other without import cycles.
export const game = {
  mode: 'loading', // loading | menu | intro | play | paused | minigame | reader | turret | cinematic | ended
  gfx: null,
  world: null,
  space: null,
  player: null,
};
