// Game state, settings and persistence. All browser storage access is guarded,
// because it can throw in private windows and sandboxed frames.
import { bus } from './events.js';

const SAVE_KEY = 'coldwake.save.v1';
const SETTINGS_KEY = 'coldwake.settings.v1';

const coarse = typeof matchMedia === 'function' && matchMedia('(pointer: coarse)').matches;
export const IS_TOUCH = coarse || (typeof navigator !== 'undefined' && navigator.maxTouchPoints > 0 && coarse);
export const IS_MOBILE =
  IS_TOUCH && typeof screen !== 'undefined' && Math.min(screen.width, screen.height) < 900;

export const defaultSettings = () => ({
  quality: IS_MOBILE ? 'medium' : 'high',
  dynamicRes: true,
  sensitivity: 1,
  invertY: false,
  fov: 78,
  master: 0.8,
  music: 0.55,
  sfx: 0.9,
  subtitles: true,
  voice: false,
  bob: true,
  hints: true,
  brightness: 1,
});

export const settings = defaultSettings();

function freshState() {
  return {
    flags: {},
    logs: [],
    transcript: [],
    visited: {},
    safeCode: String(1000 + Math.floor(Math.random() * 9000)),
    stats: { time: 0, distance: 0, minigames: 0, retries: 0 },
    pos: null,
    started: false,
    finished: false,
  };
}

export const state = freshState();

export function resetState() {
  const f = freshState();
  for (const k of Object.keys(state)) delete state[k];
  Object.assign(state, f);
}

export const has = (flag) => !!state.flags[flag];
export const count = (prefix, n) => {
  let c = 0;
  for (let i = 1; i <= n; i++) if (state.flags[prefix + i]) c++;
  return c;
};

export function setFlag(flag, value = true) {
  if (state.flags[flag] === value) return false;
  state.flags[flag] = value;
  bus.emit('flag', flag, value);
  return true;
}

function storage() {
  try {
    return window.localStorage;
  } catch {
    return null;
  }
}

export function loadSettings() {
  try {
    const raw = storage()?.getItem(SETTINGS_KEY);
    if (!raw) return;
    const data = JSON.parse(raw);
    const defaults = defaultSettings();
    for (const k of Object.keys(defaults)) {
      if (typeof data[k] === typeof defaults[k]) settings[k] = data[k];
    }
  } catch {
    /* settings are a convenience; ignore */
  }
}

export function saveSettings() {
  try {
    storage()?.setItem(SETTINGS_KEY, JSON.stringify(settings));
  } catch {
    /* ignore */
  }
}

export function hasSave() {
  try {
    const raw = storage()?.getItem(SAVE_KEY);
    if (!raw) return false;
    const data = JSON.parse(raw);
    return !!(data && data.flags && data.started && !data.finished);
  } catch {
    return false;
  }
}

export function saveGame() {
  try {
    const data = {
      flags: state.flags,
      logs: state.logs,
      transcript: state.transcript.slice(-60),
      visited: state.visited,
      safeCode: state.safeCode,
      stats: state.stats,
      pos: state.pos,
      started: state.started,
      finished: state.finished,
    };
    storage()?.setItem(SAVE_KEY, JSON.stringify(data));
    return true;
  } catch {
    return false;
  }
}

export function loadGame() {
  try {
    const raw = storage()?.getItem(SAVE_KEY);
    if (!raw) return false;
    const data = JSON.parse(raw);
    if (!data || typeof data.flags !== 'object') return false;
    resetState();
    state.flags = data.flags || {};
    state.logs = Array.isArray(data.logs) ? data.logs : [];
    state.transcript = Array.isArray(data.transcript) ? data.transcript : [];
    state.visited = data.visited || {};
    if (typeof data.safeCode === 'string' && /^\d{4}$/.test(data.safeCode)) state.safeCode = data.safeCode;
    state.stats = Object.assign(state.stats, data.stats || {});
    state.pos = data.pos || null;
    state.started = !!data.started;
    state.finished = !!data.finished;
    return true;
  } catch {
    return false;
  }
}

export function clearSave() {
  try {
    storage()?.removeItem(SAVE_KEY);
  } catch {
    /* ignore */
  }
}
