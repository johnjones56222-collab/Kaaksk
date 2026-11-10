import { buildCryo } from './cryo.js';
import { buildSpineA, buildSpineB } from './corridors.js';
import { buildHub } from './hub.js';
import { buildAICore } from './aicore.js';
import { buildQuarters } from './quarters.js';
import { buildMedbay, buildLab } from './medlab.js';
import { buildHydro } from './hydro.js';
import { buildEngineering } from './engineering.js';
import { buildCargo, buildAirlock } from './cargo.js';
import { buildBridge } from './bridge.js';
import { buildHull } from './hull.js';

export const ROOM_BUILDERS = {
  cryo: buildCryo,
  spineB: buildSpineB,
  spineA: buildSpineA,
  hub: buildHub,
  aicore: buildAICore,
  quarters: buildQuarters,
  medbay: buildMedbay,
  lab: buildLab,
  hydro: buildHydro,
  engineering: buildEngineering,
  cargo: buildCargo,
  airlock: buildAirlock,
  bridge: buildBridge,
  hull: buildHull,
};
